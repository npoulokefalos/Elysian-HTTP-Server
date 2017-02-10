#include "elysian.h"

#define ELYSIAN_WEBSOCKET_TIMEOUT_PING_MS			(10000)
#define ELYSIAN_WEBSOCKET_TIMEOUT_RX_HEALTHY_MS		(5000)

char* elysian_http_base64_encode(elysian_t* server, char *data) ;
void SHA1(char *hash_out,const char *str,int len);

elysian_websocket_def_t* elysian_websocket_def_get(elysian_t* server, char* url) {
	int k;
	if (server->websocket_def) {
		for (k = 0; (server->websocket_def[k].url != NULL) && (server->websocket_def[k].connected_handler != NULL); k++) {
			ELYSIAN_LOG("Trying to match with websocket controller %s", server->websocket_def[k].url);
			if (strcmp(server->websocket_def[k].url, url) == 0) {
				ELYSIAN_LOG("Match!")
				return (elysian_websocket_def_t*) &server->websocket_def[k];
			}
		}
	}
	return NULL;
}

elysian_err_t elysian_websockets_controller(elysian_t* server) {
	elysian_client_t* client = elysian_mvc_client(server);
	elysian_websocket_def_t* websocket_def;
	elysian_err_t err;

	ELYSIAN_LOG("[[ %s ]]", __func__);
	
	/*
	** Check if the application has registered any websocket controllers
	*/
	websocket_def = elysian_websocket_def_get(server, client->httpreq.url);
	if (!websocket_def) {
		/*
		** If the requested service is not available, the server MUST send an
		** appropriate HTTP error code (such as 404 Not Found) and abort
		** the WebSocket handshake.
		*/
		ELYSIAN_LOG("Requested websocket service not available\r\n");
		
		elysian_mvc_status_code_set(server, ELYSIAN_HTTP_STATUS_CODE_404);
		
		err = elysian_mvc_view_set(server, NULL);
		if (err != ELYSIAN_ERR_OK) { 
			return err;
		}
		
		return ELYSIAN_ERR_OK;
	}
	
	/*
	** If the server doesn't support the requested version, it MUST respond with a |Sec-WebSocket-Version| header field (or multiple
	** |Sec-WebSocket-Version| header fields) containing all versions it is willing to use.  At this point, if the client 
	** supports one of the advertised versions, it can repeat the WebSocket handshake using a new version value.
	*/
	if (client->httpreq.websocket_version != ELYSIAN_WEBSOCKET_VERSION_13) {
		ELYSIAN_LOG("Requested websocket version not supported\r\n");
		
		elysian_mvc_status_code_set(server, ELYSIAN_HTTP_STATUS_CODE_400);
		
		err = elysian_mvc_httpresp_header_add(server, "Sec-WebSocket-Version", "13");
		if(err != ELYSIAN_ERR_OK){ 
			return err;
		}
		
		err = elysian_mvc_view_set(server, NULL);
		if (err != ELYSIAN_ERR_OK) { 
			return err;
		}
		
		return ELYSIAN_ERR_OK;
	}
	
	/*
	** We do support the requested websocket version and websocket version is available
	*/
	
	client->mvc.connection = ELYSIAN_HTTP_CONNECTION_UPGRADE;
	client->mvc.connection_upgrade = ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET;
	
	elysian_mvc_transfer_encoding_set(server, ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY);
	elysian_mvc_status_code_set(server, ELYSIAN_HTTP_STATUS_CODE_101);
	client->mvc.range_start = ELYSIAN_HTTP_RANGE_WF;
	client->mvc.range_end = ELYSIAN_HTTP_RANGE_WF;
	
	char sec_websocket_accept[96];
	char websocket_accept_sha1[21];
	char* websocket_accept_sha1_base64;
	char* sec_websocket_key;
	
	
	err = elysian_mvc_httpreq_header_get(server, "Sec-WebSocket-Key", &sec_websocket_key);
	if(err != ELYSIAN_ERR_OK){ 
		return err;
	}
	
	if (strlen(sec_websocket_key) + strlen("258EAFA5-E914-47DA-95CA-C5AB0DC85B11") >= sizeof(sec_websocket_accept)) {
		/* Websocket key is invalid */
		ELYSIAN_LOG("Too long websocket key\r\n");
		return ELYSIAN_ERR_FATAL;
	}
	
	elysian_sprintf(sec_websocket_accept, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", sec_websocket_key);
	
	SHA1(websocket_accept_sha1, sec_websocket_accept, strlen(sec_websocket_accept));
	websocket_accept_sha1_base64 = elysian_http_base64_encode(server, websocket_accept_sha1);
	if (!websocket_accept_sha1_base64) {
		return ELYSIAN_ERR_POLL;
	}
	
	strcpy(sec_websocket_accept, websocket_accept_sha1_base64);
	
	elysian_mem_free(server, websocket_accept_sha1_base64);

	err = elysian_mvc_httpresp_header_add(server, "Sec-WebSocket-Accept", sec_websocket_accept);
	if(err != ELYSIAN_ERR_OK){ 
		return err;
	}
	
	err = elysian_mvc_view_set(server, NULL);
	if (err != ELYSIAN_ERR_OK) { 
		return err;
	}

	return ELYSIAN_ERR_OK;
}

elysian_websocket_frame_t* elysian_websocket_frame_allocate(elysian_t* server, uint32_t len) {
	elysian_websocket_frame_t* frame;
	
	frame = elysian_mem_malloc(server, sizeof(elysian_websocket_frame_t));
	if (!frame) {
		return NULL;
	}
	
	frame->data = elysian_mem_malloc(server, len);
	if (!frame->data) {
		elysian_mem_free(server, frame);
		return NULL;
	}
	
	return frame;
}

void elysian_websocket_frame_deallocate(elysian_t* server, elysian_websocket_frame_t* frame) {
	elysian_mem_free(server, frame->data);
	elysian_mem_free(server, frame);
}

elysian_err_t elysian_websocket_send(elysian_t* server,  elysian_websocket_opcode_e opcode, uint8_t* frame_data, uint32_t frame_len) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_websocket_frame_t* tx_frame;
	elysian_websocket_frame_t* frame;
	uint32_t index;
	
	if (frame_len > 0xffff) {
		return ELYSIAN_ERR_FATAL;
	}
	
	frame = elysian_websocket_frame_allocate(server, 2 + 2 /* if len > 125 */ + frame_len);
	if (!frame) {
		return ELYSIAN_ERR_POLL;
	}

	index = 0;
	
	frame->data[index++] = 0x80 | opcode;
	
	if (frame_len <= 125) {
		frame->data[index++] = frame_len;
	} else {
		frame->data[index++] = 126;
		frame->data[index++] = (frame_len >> 8) & 0xff;
		frame->data[index++] = (frame_len) & 0xff;
	}
	
	memcpy(&frame->data[index], frame_data, frame_len);
	frame->len = index + frame_len;
	frame->sent_len = 0;
	frame->next = NULL;
	
	if (!client->websocket.tx_frames) {
		client->websocket.tx_frames = frame;
	} else {
		tx_frame = client->websocket.tx_frames;
		while (tx_frame) {
			if (tx_frame->next == NULL) {
				tx_frame->next = frame;
				break;
			} else {
				tx_frame = tx_frame->next;
			}
		}
	}

	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_websocket_send_text(elysian_t* server,  char* frame_data, uint32_t frame_len) {
	return elysian_websocket_send(server, ELYSIAN_WEBSOCKET_FRAME_OPCODE_TEXT, (uint8_t*) frame_data, frame_len);
}

elysian_err_t elysian_websocket_send_binary(elysian_t* server, uint8_t* frame_data, uint32_t frame_len) {
	return elysian_websocket_send(server, ELYSIAN_WEBSOCKET_FRAME_OPCODE_BINARY, frame_data, frame_len);
}

elysian_err_t elysian_websocket_process_tx (elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_websocket_frame_t* tx_frame;
	uint32_t actual_sent_len;
	elysian_err_t err;
	
	ELYSIAN_LOG("elysian_websocket_process_tx()");
	
	/* Inject a close frame if the application requested a disconnection or if the peer sent a close frame */
	if ((client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) && (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_CLOSE_PENDING)) {
		err = elysian_websocket_send(server, ELYSIAN_WEBSOCKET_FRAME_OPCODE_CLOSE, NULL, 0);
		switch (err) {
			case ELYSIAN_ERR_OK:
			{
				client->websocket.flags &= ~ELYSIAN_WEBSOCKET_FLAG_CLOSE_PENDING;
			} break;
			case ELYSIAN_ERR_POLL:
			{
				return ELYSIAN_ERR_POLL;
			} break;
			case ELYSIAN_ERR_FATAL:
			default:
			{
				return ELYSIAN_ERR_FATAL;
			} break;
		};
	}
	
	/* Inject any pending ping packets */
	if (!(client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) && (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_PING_PENDING)) {
		err = elysian_websocket_send(server, ELYSIAN_WEBSOCKET_FRAME_OPCODE_PING, NULL, 0);
		switch (err) {
			case ELYSIAN_ERR_OK:
			{
				client->websocket.flags &= ~ELYSIAN_WEBSOCKET_FLAG_PING_PENDING;
			} break;
			case ELYSIAN_ERR_POLL:
			{
				return ELYSIAN_ERR_POLL;
			} break;
			case ELYSIAN_ERR_FATAL:
			default:
			{
				return ELYSIAN_ERR_FATAL;
			} break;
		};
	}
	
	/* Send any pending Tx packets */
	while (1) {
		tx_frame = client->websocket.tx_frames;
		if (!tx_frame) {
			/* No frames for transmission */
			if (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) {
				/* No pending Tx frames and we are in the 'disconnecting' state, force a disconnect */
				return ELYSIAN_ERR_FATAL;
			} else {
				return ELYSIAN_ERR_OK;
			}
		} else {
			err = elysian_socket_write(&client->socket, &tx_frame->data[tx_frame->sent_len], tx_frame->len - tx_frame->sent_len, &actual_sent_len);
			if (err == ELYSIAN_ERR_OK) {
				tx_frame->sent_len += actual_sent_len;
				if (tx_frame->sent_len == tx_frame->len) {
					/* The whole frame transmitted */
					client->websocket.tx_frames = tx_frame->next;
					elysian_websocket_frame_deallocate(server, tx_frame);
				} else {
					/* Cannot sent more frames, first frame in queue has pending tx data */
					return ELYSIAN_ERR_POLL;
				}
			} else {
				return err;
			}
		}
	}

	/* Never reach here */
	return ELYSIAN_ERR_FATAL;
}

elysian_err_t elysian_websocket_process_rx(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_websocket_frame_t* rx_frame;
	elysian_err_t err = ELYSIAN_ERR_OK;
	uint8_t poll_request = 0;
	uint8_t ignore_frame;
	
	ELYSIAN_LOG("elysian_websocket_process_rx()");
	
	/* Avoid processing if a close frame was received or disconnection was requested */
	if (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) {
		return ELYSIAN_ERR_OK;
	}
	
	/*
	** Check Rx path for newly arrived frames
	*/
	err = client->isp.func(server, &client->rcv_cbuf_list, NULL, 0);
	switch (err) {
		case ELYSIAN_ERR_OK:
		{
			/* Processing finished, new input data are not required/expected */
			ELYSIAN_ASSERT(0);
		} break;
		case ELYSIAN_ERR_READ:
		{
			/* There are not-fully received frames, should wait new elysian_schdlr_EV_READ event */
		} break;
		case ELYSIAN_ERR_POLL:
		{
			/* Pending Rx frames exist but temporary out of resources */
			poll_request = 1;
		} break;
		default:
		{
			ELYSIAN_LOG("IPS error, disconnecting!");
			return ELYSIAN_ERR_FATAL;
		} break;
	};

	/*
	** Check Rx path health status. This is to guard for quite big Rx frames that due to
	** the bounded RAM usage (or extended high load) will be never allocated succesfully.
	*/
	if (err != ELYSIAN_ERR_POLL) {
		/* Disable Rx path healh monitor */
		client->websocket.rx_path_healthy_ms = ELYSIAN_TIME_INFINITE;
	} else {
		if (client->websocket.rx_path_healthy_ms == ELYSIAN_TIME_INFINITE) {
			/* Enable Rx path healh monitor */
			client->websocket.rx_path_healthy_ms = elysian_time_now();
		} else {
			/* Rx path healh monitor enabled */
			if (elysian_time_elapsed(client->websocket.rx_path_healthy_ms) >= ELYSIAN_WEBSOCKET_TIMEOUT_RX_HEALTHY_MS) {
				/* ..and expired */
				ELYSIAN_LOG("Rx path not healthy, disconnecting!");
				return ELYSIAN_ERR_FATAL; 
			}
		}
	}
	
	/*
	** Handle Rx frames
	*/
	while (1) {
		rx_frame = client->websocket.rx_frames;
		if (!rx_frame) {
			/* No frames for transmission */
			break;
		} else {
			ELYSIAN_LOG("Frame received, opcode = 0x%x", rx_frame->data[0] & 0x0F);
			ignore_frame = 0;
			switch (rx_frame->data[0] & 0x0F) {
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_CONTINUATION:
				{
					/* Continuation frame */
				} break;
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_TEXT:
				{
					/* Text frame */
				} break;
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_BINARY:
				{
					/* Binary frame */
				} break;
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_CLOSE:
				{
					/* Close frame */
					client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_CLOSE_RECEIVED;
					client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING;
				} break;
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_PING:
				{
					/* Ping frame (not expected) */
					ignore_frame = 1;
				} break;
				case ELYSIAN_WEBSOCKET_FRAME_OPCODE_PONG:
				{
					/* Pong frame */
					client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_PONG_RECEIVED;
				} break;
				default:
				{
					/* Reserved, ignore */
					ignore_frame = 1;
				} break;
			}
			
			if (!ignore_frame) {
				if ((rx_frame->len > 1) && (client->websocket.def->frame_handler)) {
					err = client->websocket.def->frame_handler(server, client->websocket.handler_args, &rx_frame->data[1], rx_frame->len - 1);
					if (err != ELYSIAN_ERR_OK) {
						/* Application rquested disconnection */
						client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING;
					} else {
						/* A Tx packet might have benn sent, enable fast poll */
						poll_request = 1;
					}
				} else {
					/* Ignore the frame */
				}
			} else {
				/* Ignore the frame */
			}

			client->websocket.rx_frames = rx_frame->next;
			elysian_websocket_frame_deallocate(server, rx_frame);
			
			if (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) {
				break;
			}
		}	
	}

	if (poll_request) {
		return ELYSIAN_ERR_POLL;
	} else {
		return ELYSIAN_ERR_OK;
	}
}

elysian_err_t elysian_websocket_process(elysian_t* server) {
	uint8_t poll_request;
	elysian_err_t err;
	
	/*
	** Process Rx frames. Tx Frames might be generated through app frame Rx handler
	*/
	err = elysian_websocket_process_rx(server);
	switch (err) {
		case ELYSIAN_ERR_OK:
			/* No pending Rx frames */
			break;
		case ELYSIAN_ERR_READ:
			/* There are not fully received frames, should wait new elysian_schdlr_EV_READ event */
			break;
		case ELYSIAN_ERR_POLL:
			/* Pending Rx frames exist */
			poll_request = 1;
			break;
		case ELYSIAN_ERR_FATAL:
		default:
			return ELYSIAN_ERR_FATAL;
			break;
	};		  

	/*
	** Process Tx frames
	*/
	err = elysian_websocket_process_tx(server);
	switch (err) {
		case ELYSIAN_ERR_OK:
			/* No pending Tx frames */
			break;
		case ELYSIAN_ERR_POLL:
			/* Pending Tx frames exist */
			poll_request = 1;
			break;
		case ELYSIAN_ERR_FATAL:
		default:
			return ELYSIAN_ERR_FATAL;
			break;
	};
	
	if (poll_request) {
		return ELYSIAN_ERR_POLL;
	} else {
		return ELYSIAN_ERR_OK;
	}
}

elysian_err_t elysian_websocket_timer_config(elysian_t* server, uint32_t timer_interval_ms) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	if (timer_interval_ms) {
		client->websocket.timer_interval_ms = timer_interval_ms;
		elysian_schdlr_state_timer1_set(server, client->websocket.timer_interval_ms);
		return ELYSIAN_ERR_OK;
	} else {
		return ELYSIAN_ERR_FATAL;
	}
}

elysian_err_t elysian_websocket_app_timer(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	/* Avoid processing if a close frame was received or disconnection was requested */
	if (client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING) {
		return ELYSIAN_ERR_OK;
	}
	
	if (client->websocket.def->timer_handler) {
		err = client->websocket.def->timer_handler(server, client->websocket.handler_args);
		if (err != ELYSIAN_ERR_OK) {
			/* Application rquested disconnection */
			client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING;
		}
	} 
	
	return ELYSIAN_ERR_OK;
};

elysian_err_t elysian_websocket_ping_timer(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);

	if ((client->websocket.flags & ELYSIAN_WEBSOCKET_FLAG_PONG_RECEIVED) == 0) {
		/* Peer did not responded to our last ping, abort */
		return ELYSIAN_ERR_FATAL;
	} else {
		client->websocket.flags &=~ ELYSIAN_WEBSOCKET_FLAG_PONG_RECEIVED;
		client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_PING_PENDING;
		elysian_schdlr_state_timer2_set(server, ELYSIAN_WEBSOCKET_TIMEOUT_PING_MS);
	}

	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_websocket_connected(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;

	elysian_schdlr_state_timer2_set(server, ELYSIAN_WEBSOCKET_TIMEOUT_PING_MS);
	
	client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_PING_PENDING;
	client->websocket.flags |= ELYSIAN_WEBSOCKET_FLAG_PONG_RECEIVED;
	client->websocket.timer_interval_ms = ELYSIAN_TIME_INFINITE;
	client->websocket.rx_path_healthy_ms = ELYSIAN_TIME_INFINITE;
	
	client->websocket.def = elysian_websocket_def_get(server, client->httpreq.url);
	if (!client->websocket.def) {
		ELYSIAN_ASSERT(0);
		return ELYSIAN_ERR_FATAL;
	}
	
	if (client->websocket.def->connected_handler) {
		err = client->websocket.def->connected_handler(server, &client->websocket.handler_args);
		if (err != ELYSIAN_ERR_OK) {
			/* Don't call disconnected handler if application blocked connection */
			client->websocket.def = NULL;
			return ELYSIAN_ERR_FATAL;
		} else {
			return ELYSIAN_ERR_OK;
		}
	} else {
		return ELYSIAN_ERR_FATAL;
	}
	
	return ELYSIAN_ERR_FATAL;
}

elysian_err_t elysian_websocket_init(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);

	client->websocket.def = NULL;
	client->websocket.handler_args = NULL;
	client->websocket.flags = 0;
	client->websocket.rx_frames = NULL;
	client->websocket.tx_frames = NULL;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_websocket_cleanup(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_websocket_frame_t* frame;
	
	if (client->websocket.def) {
		if (client->websocket.def->disconnected_handler) {
			client->websocket.def->disconnected_handler(server, client->websocket.handler_args);
		}
		client->websocket.def = NULL;
	}
	
	while (client->websocket.tx_frames) {
		frame = client->websocket.tx_frames;
		client->websocket.tx_frames = client->websocket.tx_frames->next;
		elysian_websocket_frame_deallocate(server, frame);
	};
	
	while (client->websocket.rx_frames) {
		frame = client->websocket.rx_frames;
		client->websocket.rx_frames = client->websocket.rx_frames->next;
		elysian_websocket_frame_deallocate(server, frame);
	};

	return ELYSIAN_ERR_OK;
}
