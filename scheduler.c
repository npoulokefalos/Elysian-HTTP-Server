/*
 * This file is part of Elysian Web Server
 *
 * Copyright (C) 2016,  Nikos Poulokefalos, npoulokefalos at gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "elysian.h"

/*
** Specify how often the Web Server thread is going to suspend itself to ensure that other threads with same priority 
** are not starving. This is to protect from wrong priority assignments..
*/
#define ELYSIAN_YIELD_INTERVAL_MS  	(33)

/*
** The maximum interval that is not so big to be perceived 
** as sluggish by humans and not unnecessairy small either.
*/
#define ELYSIAN_333_INTERVAL_MS		(333)

#define ELYSIAN_STARVATION_ENABLED	(1)

void elysian_schdlr_throw_event(elysian_t* server, elysian_schdlr_task_t* task, elysian_schdlr_ev_t ev){
    elysian_schdlr_t* schdlr = &server->scheduler;
	if(!task->state){
        /*
        ** Task made a quit request and will be removed shortly..
        */
        return;
    }

	schdlr->current_task = task;
    task->state(server, ev);
	schdlr->current_task = NULL;
    
    while(task->state != task->new_state){
        task->state = task->new_state;

		schdlr->current_task = task;
        elysian_schdlr_state_poll_set(server, ELYSIAN_TIME_INFINITE);
		elysian_schdlr_state_timeout_set(server, ELYSIAN_TIME_INFINITE);
        elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_NORMAL);
		schdlr->current_task = NULL;
		
        if(!task->state){
            /* Task quit request */
			if(task->cbuf_list){
                elysian_cbuf_list_free(server, task->cbuf_list);
                task->cbuf_list = NULL;
            }

            if(!task->client->socket.actively_closed){
                elysian_socket_close(&task->client->socket);
                task->client->socket.actively_closed = 1;
            }
            if(task->client){
                elysian_mem_free(server, task->client);
                task->client = NULL;
            }

            ELYSIAN_LOG("TASK WAS REMOVED!!!!!!!!!!!!!!!!!");
            return;
        }else{
			schdlr->current_task = task;
            task->state(server, elysian_schdlr_EV_ENTRY);
			schdlr->current_task = NULL;
        }
    }

    /* Not an error but its a good practice to set a timeout.. */
    ELYSIAN_ASSERT(task->timeout_delta != ELYSIAN_TIME_INFINITE, "");
}

#define ELYSIAN_NOMEM_SLEEP_INTERVAL_MS	(33)
uint32_t elysian_schdlr_free_client_slots(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->tasks.next;
	uint32_t max_client_slots = ((sizeof(schdlr->socket_readset)/sizeof(schdlr->socket_readset[0])) - 1 /* Server */);
	while(task != &schdlr->tasks){
		ELYSIAN_ASSERT(max_client_slots, "");
		max_client_slots--;
		task = task->next;
	}
	return max_client_slots;
}

elysian_schdlr_task_t* elysian_schdlr_get_task(elysian_t* server, elysian_socket_t* socket){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->tasks.next;
	while(task != &schdlr->tasks){
		if(&task->client->socket == socket){
			return task;
		}
		task = task->next;
	}
	return NULL;
}

elysian_schdlr_task_t* elysian_schdlr_get_lowest_priority_task(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->tasks.next;
	elysian_schdlr_task_t* lowest_priority_task;
	if(task == &schdlr->tasks){
		return NULL;
	}
	lowest_priority_task = task;
	while(task != &schdlr->tasks){
		lowest_priority_task = (lowest_priority_task->priority > task->priority) ? task : lowest_priority_task;
		task = task->next;
	}
	return lowest_priority_task;
}



uint32_t elysian_schdlr_elapsed_time(uint32_t tic_ms){
	uint32_t toc_ms;
	uint32_t elapsed_ms;
	
	toc_ms = elysian_time_now();
	elapsed_ms = (toc_ms >= tic_ms) ? toc_ms - tic_ms : toc_ms;
	
	return elapsed_ms;
}

void elysian_schdlr_exec_socket_events(elysian_t* server, uint32_t interval_ms){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_client_t* new_client;
	elysian_schdlr_task_t* task;
	elysian_schdlr_task_t* new_task;
	elysian_schdlr_task_t* last_task;
	elysian_cbuf_t* new_cbuf;
	uint32_t tic_ms;
	uint32_t backoff_ms;
	uint32_t fdset_size;
	uint32_t socket_events;
	uint32_t elapsed_ms;
	uint32_t accept_ms;
	elysian_err_t err;
	elysian_socket_t client_socket;
	uint32_t i;
	
	tic_ms = elysian_time_now();
	backoff_ms = 0;
	
	do{
		fdset_size = 0;
		socket_events = 0;
		
		new_client = elysian_mem_malloc(server, sizeof(elysian_client_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
		new_task = elysian_mem_malloc(server, sizeof(elysian_schdlr_task_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
		new_cbuf = elysian_cbuf_alloc(server, NULL, ELYSIAN_CBUF_LEN);
		if(!new_cbuf){
			/*
			** Maybe some connection was closed, give a chance to read and detect this
			** if ELYSIAN_CBUF_LEN has been set to a too big value.
			*/
			new_cbuf = elysian_cbuf_alloc(server, NULL, 32);
		}

		if(new_task && new_client){
			/*
			** No memory restriction to accept a new client
			*/
			schdlr->socket_readset[fdset_size] = &schdlr->socket;
#if (ELYSIAN_SOCKET_SELECT_SUPPORTED == 1)
			schdlr->socket_readset_status[fdset_size] = 0;
#else
			schdlr->socket_readset_status[fdset_size] = 1;
#endif
			fdset_size++;
		}else{
			if(new_client){
				elysian_mem_free(server, new_client);
				new_client = NULL;
			}
			if(new_task){
				elysian_mem_free(server, new_task);
				new_task = NULL;
			}
		}
		
		if(new_cbuf){		
			/*
			** No memory restriction to receive new data
			*/
			task = schdlr->tasks.next;
			while(task != &schdlr->tasks){
				/*
				** During loop, some client may have been removed from 
				** elysian_schdlr_EV_READ / elysian_schdlr_EV_ABORT events
				*/
				if (task->client) {
					if((task->client->socket.passively_closed == 0) && (task->client->socket.actively_closed == 0)){
						schdlr->socket_readset[fdset_size] = &task->client->socket;
#if (ELYSIAN_SOCKET_SELECT_SUPPORTED == 1)
						schdlr->socket_readset_status[fdset_size] = 0;
#else
						schdlr->socket_readset_status[fdset_size] = 1;
#endif
						fdset_size++;
					}
				}
				task = task->next;
			}
		}
		
		if(fdset_size){
#if (ELYSIAN_SOCKET_SELECT_SUPPORTED == 1)
			elapsed_ms = elysian_schdlr_elapsed_time(tic_ms);
			accept_ms = (interval_ms > elapsed_ms) ? (interval_ms - elapsed_ms) : 0;
			err = elysian_socket_select(schdlr->socket_readset, fdset_size, accept_ms, schdlr->socket_readset_status);
			if(err != ELYSIAN_ERR_OK){
				for(i = 0; i < fdset_size; i++){
					schdlr->socket_readset_status[i] = 0;
				}
			}
#else

#endif
			for(i = 0; i < fdset_size; i++){
				if(!schdlr->socket_readset_status[i]){
					continue;
				}
				
				if(schdlr->socket_readset[i] == &schdlr->socket){
						/*
						** Server event
						*/
						err = elysian_socket_accept(&schdlr->socket, 0, &client_socket);
						if(err != ELYSIAN_ERR_OK){
							ELYSIAN_LOG("Client rejected due to accept error!");
							continue;
						}
						
						socket_events++;
						
						if(!elysian_schdlr_free_client_slots(server)){
							ELYSIAN_LOG("Client Rejected: Limit of %u clients has been reached!", ELYSIAN_MAX_CLIENTS_NUM);
							elysian_socket_close(&client_socket); 
						}else{
							client_socket.passively_closed = 0;
							client_socket.actively_closed = 0;
							//new_task->allocated_memory = 0;
							new_task->client = new_client;
							new_task->client->socket = client_socket;
							
							schdlr->current_task = new_task;
							elysian_schdlr_state_poll_set(server, ELYSIAN_TIME_INFINITE);
							elysian_schdlr_state_timeout_set(server, ELYSIAN_TIME_INFINITE);
							elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_NORMAL);
							schdlr->current_task = NULL;
							
							new_task->cbuf_list = NULL;
							new_task->state = schdlr->client_connected_state;
							new_task->new_state = new_task->state;
							
							/* 
							** Add client to the last position of the Client list
							** Before: last_task -> HEAD
							** After:  last_task -> task -> HEAD
							*/
							last_task = schdlr->tasks.prev;
							new_task->next = last_task->next;
							new_task->prev = last_task;
							last_task->next->prev = new_task;
							last_task->next = new_task;
							
							elysian_schdlr_throw_event(server, new_task, elysian_schdlr_EV_ENTRY);
							
							/*
							** Invalidate <task> and <client> alocations as they have been used and should not be freed.
							*/
							new_client = NULL;
							new_task = NULL;
						}
					}else{
						/*
						** Client event
						*/
						task = elysian_schdlr_get_task(server, schdlr->socket_readset[i]);
						if(!task){
							ELYSIAN_ASSERT(task, "");
							continue;
						}
						
						if (!task->client) {
							/* Socket event for a removed client ? */
							ELYSIAN_ASSERT(0, "");
							continue;
						}
						
						if(task->client->socket.passively_closed || task->client->socket.actively_closed){
							ELYSIAN_ASSERT(0, ""); /* This can never happen ??? */
							continue;
						}

						/* READ event */
						elysian_err_t err;
						uint32_t received;
						elysian_cbuf_t* cbuf_alt;
						if(!new_cbuf){
							new_cbuf = elysian_cbuf_alloc(server, NULL, ELYSIAN_CBUF_LEN);
							if(!new_cbuf){
								/*
								** Maybe some connection was closed, give a chance to read and detect this
								** if ELYSIAN_CBUF_LEN has been set to a too big value.
								*/
								new_cbuf = elysian_cbuf_alloc(server, NULL, 32);
							}
						}

						if(new_cbuf){
							err = elysian_socket_read(&task->client->socket, new_cbuf->data, new_cbuf->len, &received);
							switch(err){
								case ELYSIAN_ERR_OK:
								{
									socket_events++;
									if((received * 100)/(new_cbuf->len) < 85){
										cbuf_alt = elysian_cbuf_alloc(server, new_cbuf->data, received);
										if(cbuf_alt){
											elysian_cbuf_free(server, new_cbuf);
											new_cbuf = cbuf_alt;
										}
									}
									new_cbuf->len = received;
									elysian_cbuf_list_append(&task->cbuf_list, new_cbuf);
									elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_READ);
									/*
									** Invalidate <cbuf> alocation as it has been used and should not be freed.
									*/
									new_cbuf = NULL;
								}break;
								case ELYSIAN_ERR_POLL:
								{
								}break;
								default:
								{
									task->client->socket.passively_closed = 1;
									ELYSIAN_LOG("Connection pasively closed!!!");
									elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_ABORT);
								}break;
							};
						}
					}
			} //for
		}
		
		if(new_task){
			elysian_mem_free(server, new_task);
			new_task = NULL;
		}
		if(new_client){
			elysian_mem_free(server, new_client);
			new_client = NULL;
		}
		if(new_cbuf){
			elysian_mem_free(server, new_cbuf);
			new_cbuf = NULL;
		}
		
		if(socket_events){
			/*
			** One or more socket events took place, stop.
			*/
			return;
		}else{
			/*
			** No socket events were received
			*/
			elapsed_ms = elysian_schdlr_elapsed_time(tic_ms);
			if(elapsed_ms >= interval_ms){
				return;
			}else{
				if(!fdset_size){
					/*
					** No memory to process socket events, just sleep and check for deadlock.
					*/
					if(interval_ms > elapsed_ms){
						elysian_time_sleep(interval_ms - elapsed_ms);
					}
					ELYSIAN_LOG("CHECK Memory starvation HERE !!!");
					return;
				}else{
					/*
					** There is memory to process socket events, but no event received yet. 
					** Backoff and retry.
					*/
					uint32_t ELYSIAN_POLLING_INTERVAL_MS = ELYSIAN_333_INTERVAL_MS;
					backoff_ms = (backoff_ms == 0) ? 1 : backoff_ms * 2;
					backoff_ms = (backoff_ms >  (interval_ms - elapsed_ms)) ? (interval_ms - elapsed_ms) : backoff_ms;
					backoff_ms = (backoff_ms > ELYSIAN_POLLING_INTERVAL_MS) ? ELYSIAN_POLLING_INTERVAL_MS : backoff_ms;
					elysian_time_sleep(backoff_ms);
				}
			}
		}
	} while(1);

}

void elysian_schdlr_exec_immediate_events(elysian_t* server, uint32_t* max_sleep_ms){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task;
	elysian_schdlr_task_t* next_task;
#if (ELYSIAN_STARVATION_ENABLED == 1)
	uint8_t starvation_detected;
	uint32_t polling_tasks;
	uint32_t polling_tasks_starving;
	polling_tasks = 0;
	polling_tasks_starving = 0;
	starvation_detected = 0;
#endif
	
	/*
    ** Remove all clients with NULL state
    */
	*max_sleep_ms = -1;
    task = schdlr->tasks.next;
    while(task != &schdlr->tasks){
        next_task = task->next;

        if(!task->state){
            // Before task->prev -> task -> task->next
            // After task->prev -> task->next
            task->prev->next = task->next;
            task->next->prev = task->prev;
			//ELYSIAN_LOG("Memory leak of task is %u bytes", task->allocated_memory);
            //ELYSIAN_ASSERT(task->allocated_memory == 0, "Possible memory leak!");
            elysian_mem_free(server, task);
        }else{
            if(!task->poll_delta){
                elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_POLL);
            }
			if(!task->timeout_delta){
				//elysian_schdlr_set_task_state_timeout(task, ELYSIAN_TIME_INFINITE);
				ELYSIAN_LOG("Timeout reached!!!");
				elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_ABORT);
			}
			if(!task->state){
				/*
				** Remove client if processing has been finished
				*/
				continue;
			}
			
			/*
			** Calculate max sleeping interval
			*/
			*max_sleep_ms = (*max_sleep_ms > task->poll_delta) ? task->poll_delta : *max_sleep_ms;
			*max_sleep_ms = (*max_sleep_ms > task->timeout_delta) ? task->timeout_delta : *max_sleep_ms;
		
#if (ELYSIAN_STARVATION_ENABLED == 1)
			if (task->poll_delta != ELYSIAN_TIME_INFINITE) {
				polling_tasks++;
				if (task->poll_delta != 0) {
					/*
					** This task temporary suffers from resource starvation (memory, network)
					*/
					polling_tasks_starving++;
				}
			}
#endif
        }
        task = next_task;
    }

#if (ELYSIAN_STARVATION_ENABLED == 1)
	/*
	** We detect memory starvation if all polling tasks are startving, and
	** there are at least two such tasks. A single task should be allowed 
	** to starve according to its state machine. It should not be a problem
	** since it does not infuence other tasks..
	*/
	if( (polling_tasks_starving == polling_tasks) && (polling_tasks > 1)) {
		starvation_detected = 1;
	}
	
	/*
	** If we are detecting memory starvation for a fairly long
	** period, remove the lowest priority task.
	*/
	if (!starvation_detected) {
		server->starvation_detection_t0 = ELYSIAN_TIME_INFINITE;
	} else {
		starvation_detected = 0;
		if (server->starvation_detection_t0 == ELYSIAN_TIME_INFINITE) {
			server->starvation_detection_t0 = elysian_time_now();
		} else if(elysian_schdlr_elapsed_time(server->starvation_detection_t0) > ELYSIAN_333_INTERVAL_MS) {
			/*
			** Srarvation detected, try to recover and initialize starvation detection procedure from the beginning
			*/
			server->starvation_detection_t0 = ELYSIAN_TIME_INFINITE;
			starvation_detected = 1;
		}
	}
	
	if (starvation_detected) {
		ELYSIAN_LOG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		ELYSIAN_LOG("Memory starvation detected !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		ELYSIAN_LOG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		task = elysian_schdlr_get_lowest_priority_task(server);
		ELYSIAN_ASSERT(task != NULL, "");
		elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_ABORT);
		if (task->state == NULL) {
			/*
			** Client aborted, remove the task
			*/
			
			// Before task->prev -> task -> task->next
			// After task->prev -> task->next
			task->prev->next = task->next;
			task->next->prev = task->prev;
			
			elysian_mem_free(server, task);
			
			/*
			** Enable fast polling for all polling tasks
			*/
			task = schdlr->tasks.next;
			while(task != &schdlr->tasks){
				if(task->poll_delta != ELYSIAN_TIME_INFINITE){
					task->poll_delta = 0;
				}
				task = task->next;;
			} 
		}
	}
#endif
	
    /*
    ** Roundrobin re-scheduling. This is done to avoid giving the highest priority
    ** to the first task of our task list, as this is the task that will be firstly
    ** served through the use of select() and is possibly going to consume TCP/IP's
    ** memory because of the socket_write() api call overuse.
    ** Before: HEAD -> first_task -> xxx -> last_task
    ** After: HEAD -> xxx -> last_task -> first_task
    */
    elysian_schdlr_task_t* first_task;
	elysian_schdlr_task_t* last_task;
    first_task = schdlr->tasks.next;
    last_task = schdlr->tasks.prev;
    if((first_task != last_task) && (first_task != &schdlr->tasks) && (last_task != &schdlr->tasks)){
        /*
        ** Remove first task
        */
        schdlr->tasks.next = first_task->next;
        first_task->next->prev = &schdlr->tasks;
        /*
        ** Add first task at last position
        */
        first_task->next = last_task->next;
        first_task->prev = last_task;
        last_task->next->prev = first_task;
        last_task->next = first_task;
    }
}


void elysian_schdlr_exec(elysian_t* server, uint32_t poll_period_ms){
	uint32_t sleep_period_ms;
	
	elysian_schdlr_exec_immediate_events(server, &sleep_period_ms);
	
	poll_period_ms = poll_period_ms >= sleep_period_ms ? sleep_period_ms : poll_period_ms;
	
	elysian_schdlr_exec_socket_events(server, poll_period_ms);
}

uint32_t elysian_schdlr_time_correction(elysian_t* server, uint32_t* tic_ms){
	elysian_schdlr_t* schdlr = &server->scheduler;
    elysian_schdlr_task_t* task;
    uint32_t calibration_delta;
    //uint32_t toc_ms;
    
    //toc_ms = elysian_time_now();
	//calibration_delta = (toc_ms >= (*tic_ms)) ? toc_ms - (*tic_ms) : toc_ms;
	calibration_delta = elysian_schdlr_elapsed_time(*tic_ms);
    if(!calibration_delta){
        return 0;
    }
    *tic_ms = elysian_time_now();


    task = schdlr->tasks.next;
    while(task != &schdlr->tasks){
        if(task->poll_delta != ELYSIAN_TIME_INFINITE){
            task->poll_delta = (task->poll_delta > calibration_delta) ? task->poll_delta - calibration_delta : 0;
        }
        if(task->timeout_delta != ELYSIAN_TIME_INFINITE){
            task->timeout_delta = (task->timeout_delta > calibration_delta) ? task->timeout_delta - calibration_delta : 0;
        }
        task = task->next;
    }

    return calibration_delta;
}


void elysian_schdlr_yield(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	uint32_t yield_delta;
	uint32_t current_timestamp;
	current_timestamp = elysian_time_now();
	yield_delta = (current_timestamp >= schdlr->prev_yield_timestamp) ? current_timestamp - schdlr->prev_yield_timestamp : current_timestamp;
	if(yield_delta >= ELYSIAN_YIELD_INTERVAL_MS){
		elysian_thread_yield();
		schdlr->prev_yield_timestamp = elysian_time_now();
	}
}

/* -------------------------------------------------------------------------------------------------------------
* API
 ------------------------------------------------------------------------------------------------------------- */
elysian_schdlr_task_t* elysian_schdlr_current_task_get(elysian_t* server){
	if(server) {
		return server->scheduler.current_task;
	} else {
		return NULL;
	}
}

elysian_client_t* elysian_schdlr_current_client_get(elysian_t* server){
	return server->scheduler.current_task->client;
}

#if 0
elysian_schdlr_task_t* elysian_schdlr_current_task_get(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	return schdlr->current_task;
}
#endif

void elysian_schdlr_state_set(elysian_t* server, elysian_schdlr_state_t state){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
    ELYSIAN_ASSERT(task != NULL, "");
    task->new_state = state;
}

elysian_schdlr_state_t elysian_schdlr_state_get(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
    ELYSIAN_ASSERT(task != NULL, "");
    return task->state;
}

void elysian_schdlr_state_poll_set(elysian_t* server, uint32_t poll_delta){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
	ELYSIAN_ASSERT(task != NULL, "");
    task->poll_delta = poll_delta;
    task->poll_delta_init = task->poll_delta;
}

void elysian_schdlr_state_poll_enable(elysian_t* server){
    elysian_schdlr_state_poll_set(server, 0);
}

void elysian_schdlr_state_poll_disable(elysian_t* server){
    elysian_schdlr_state_poll_set(server, ELYSIAN_TIME_INFINITE);
}

void elysian_schdlr_state_poll_backoff(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
	uint32_t new_delta;
	
	ELYSIAN_ASSERT(task != NULL, "");
	ELYSIAN_ASSERT(task->poll_delta_init != ELYSIAN_TIME_INFINITE, "");
	
#define MAX_POLL_BACKOFF (ELYSIAN_333_INTERVAL_MS)
	new_delta = ((task->poll_delta_init == 0) ? 1 : (((task->poll_delta_init) < (MAX_POLL_BACKOFF / 2)) ? (task->poll_delta_init * 2) : (MAX_POLL_BACKOFF)));
	
	ELYSIAN_LOG("POLL DELTA %u -> %u", schdlr->current_task->poll_delta_init, new_delta);
    elysian_schdlr_state_poll_set(server, new_delta);
}

void elysian_schdlr_state_timeout_set(elysian_t* server, uint32_t timeout_delta){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
    ELYSIAN_ASSERT(task != NULL, "");
    task->timeout_delta = timeout_delta;
	task->timeout_delta_init = task->timeout_delta;
}

void elysian_schdlr_state_timeout_reset(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
    ELYSIAN_ASSERT(task != NULL, "");
    task->timeout_delta = task->timeout_delta_init;
}

void elysian_schdlr_state_priority_set(elysian_t* server, elysian_schdlr_task_prio_t priority){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
	ELYSIAN_ASSERT(task != NULL, "");
	task->priority = priority;
}

elysian_cbuf_t* elysian_schdlr_state_socket_read(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task = schdlr->current_task;
    elysian_cbuf_t* cbuf_list;
	ELYSIAN_ASSERT(task != NULL, "");
    cbuf_list = task->cbuf_list;
    task->cbuf_list = NULL;
	
	//task->allocated_memory += elysian_cbuf_list_len(cbuf_list);
    return cbuf_list;
}

void elysian_schdlr_poll(elysian_t* server, uint32_t intervalms){
	elysian_schdlr_t* schdlr = &server->scheduler;
    uint32_t tic_ms;
    uint32_t calibration_ms;
    
	/*
	** Calibrate deltas due to application delays
	*/
    elysian_schdlr_time_correction(server, &schdlr->non_poll_tic_ms);
    
	/*
	** Poll for the desired interval
	*/
	tic_ms = elysian_time_now();
	while(intervalms){
		/*
		** Make sure WS does not monopolize the CPU
		*/
		elysian_schdlr_yield(server);
		
        /*
		** Execute tasks
        */
        elysian_schdlr_exec(server, intervalms);
		
        /*
		** Callibrate deltas
        */
        calibration_ms = elysian_schdlr_time_correction(server, &tic_ms);
        intervalms = (intervalms >= calibration_ms) ? intervalms - calibration_ms : 0;
	}

	schdlr->non_poll_tic_ms = elysian_time_now();
}

void elysian_schdlr_stop(elysian_t* server){
	elysian_schdlr_t* schdlr = &server->scheduler;
	elysian_schdlr_task_t* task;
	elysian_schdlr_task_t* next_task;
	
	if(!schdlr->socket.actively_closed){
		elysian_socket_close(&schdlr->socket);
		schdlr->socket.actively_closed = 1;
	}
			
	task = schdlr->tasks.next;
    while(task != &schdlr->tasks){
        next_task = task->next;
        if(!task->state){
            /*
            ** Do we have to release the client?
            */
            
            // Before task->prev -> task -> task->next
            // After task->prev -> task->next
            task->prev->next = task->next;
            task->next->prev = task->prev;
            
            elysian_mem_free(server, task);
        }else{
			/*
			** Throw an ABORT event
			*/
			elysian_schdlr_throw_event(server, task, elysian_schdlr_EV_ABORT);
			
			if(!task->state){
				continue;
			}
        }
        task = next_task;
    }
}

elysian_err_t elysian_schdlr_init(elysian_t* server, uint16_t port, elysian_schdlr_state_t client_connected_state){
	elysian_schdlr_t* schdlr = &server->scheduler;
    elysian_err_t err;
    
    ELYSIAN_ASSERT(schdlr != NULL,"");
    ELYSIAN_ASSERT(client_connected_state != NULL,"");
    
	schdlr->server = server;
	
    schdlr->current_task = NULL;
    
	schdlr->prev_yield_timestamp = elysian_time_now();
	
	schdlr->non_poll_tic_ms = elysian_time_now();
    schdlr->client_connected_state = client_connected_state;
    
	//elysian_schdlr_set_disabled_acceptor_delta(schdlr, 0);
	//elysian_schdlr_set_disabled_reader_delta(schdlr, 0);
	
    schdlr->tasks.next = schdlr->tasks.prev = &schdlr->tasks;
    schdlr->tasks.state = (elysian_schdlr_state_t) !NULL; // Don't care
    schdlr->tasks.new_state = NULL;
    //elysian_schdlr_set_task_state_poll(&schdlr->tasks, ELYSIAN_TIME_INFINITE); // Don't care
	//elysian_schdlr_set_task_state_timeout(&schdlr->tasks, ELYSIAN_TIME_INFINITE); // Don't care

    err = elysian_socket_listen(port, &schdlr->socket);
    if(err != ELYSIAN_ERR_OK){
        ELYSIAN_LOG_ERR("Listen error!");
        while(1){}
        schdlr->socket.passively_closed = 1;
        schdlr->socket.actively_closed = 1;
        return ELYSIAN_ERR_FATAL;
    }
    
    schdlr->socket.passively_closed = 0;
    schdlr->socket.actively_closed = 0;
    
    return ELYSIAN_ERR_OK;
}
