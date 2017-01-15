import os
import binascii

def filepath2filename(filepath):
	path, name = os.path.split(filepath)
	return name

def filepath2path(filepath):
	path, name = os.path.split(filepath)
	if path == "":
		path = "."
	return path
	
def filepath2variable(filepath):
	return 'data_' + filepath2filename(filepath).replace(' ','_').replace('.','_').replace('-','_').replace('(','_').replace(')','_')

def filepath2size(filepath):
	return os.path.getsize(filepath)

WORKING_DIRECTORY=filepath2path(__file__)
OUTPUT_FILE = WORKING_DIRECTORY + '/fsdata.c'

print '\r\n'
print 'Creating filesystem for directory "' + WORKING_DIRECTORY + '"'
print 'Filesystem will be saved into "' + OUTPUT_FILE + '"'
print '\r\n'

fd_dst = open(OUTPUT_FILE, "w")
fd_dst.write('/*\r\n** Elysian Web Server, ' + filepath2filename(OUTPUT_FILE) + '\r\n*/\r\n\r\n')

processed_filepaths = []
fs_size = 0
fs_count = 0
for root, directories, filenames in os.walk(WORKING_DIRECTORY):
	for filename in filenames:
		# Ignore backup files
		if filename.startswith('~'):
			continue
		# Ignore this file
		if filename == filepath2filename(__file__):
			continue
		# Ignore output file
		if filename == filepath2filename(OUTPUT_FILE):
			continue
		print os.path.join(root, filename) + " [" + str(os.path.getsize(os.path.join(root, filename))) + " bytes]"
		processed_filepaths.append(os.path.join(root, filename))
		fs_size += filepath2size(os.path.join(root, filename))
		fs_count += 1
		fd_dst.write('static const char ' + filepath2variable(os.path.join(root, filename)) + '[] = {')
		with open(os.path.join(root, filename), "rb") as fd_src:
			byte = fd_src.read(1)
			while byte != "":
				fd_dst.write('0x')
				fd_dst.write(binascii.hexlify(byte)) 
				fd_dst.write(',')
				byte = fd_src.read(1)
		fd_dst.write('};\r\n')

fd_dst.write('\r\n\r\nconst elysian_file_rom_t rom_fs[] = { /* ' + str(fs_size) +' bytes total */\r\n')
for filepath in processed_filepaths:
	fd_dst.write('\t{.name = (char*) \"' + filepath[len(WORKING_DIRECTORY):] + '\", .ptr = (uint8_t*)' + filepath2variable(filepath) +', .size = sizeof(' + filepath2variable(filepath) + ')}, /* ' + str(filepath2size(filepath)) + ' bytes */\r\n');
#len(fs_ROM_VRT_ROOT)
fd_dst.write('\t{.name = (char*) NULL, .ptr = (uint8_t*) NULL, .size = 0}, /* End of FS */\r\n');
fd_dst.write('};\r\n');
fd_dst.close()

print '\r\n'
print 'Filesystem created, ' + str(fs_count) + ' files, ' + str(fs_size) +' bytes total\r\n'