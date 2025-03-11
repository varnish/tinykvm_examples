import os
import mimetypes
import sys
import zlib

location = sys.argv[1]
output_c = sys.argv[2]
mime = mimetypes.MimeTypes()

file = open(output_c, "w")
file.write('#define KVM_API_ALREADY_DEFINED\n')
file.write('#include "src/api.h"\n')
file.write('#include "src/crc32.h"\n')
file.write('#include <stdio.h>\n')
file.write('\n')

binaries = []
cases = []
bins = 0

aliases = []
with open('aliases.txt','r') as f:
    for line in f:
        words = line.split()
        name = bytes(words[0], 'utf-8')
        dest = bytes(words[1], 'utf-8')
        print("Alias ", name, " goes to: ", dest)
        src_crc = hex(zlib.crc32(name) & 0xffffffff)
        dst_crc = hex(zlib.crc32(dest) & 0xffffffff)
        # Create alias cases
        cases.append('  case ' + src_crc + ':\n')
        cases.append('    crc = ' + dst_crc + ';\n')
        cases.append('    goto restart;\n')

for path, subdirs, files in os.walk(location):
    for name in files:
        fullpath = os.path.join(path, name)

        ## embed file as resource0, resource1 etc.
        bname = "resource" + str(bins)
        bins = bins + 1
        binaries.append('EMBED_BINARY(' + bname + ', "' + fullpath + '")\n')

        ## create CRC32 of /resource/path
        rel = os.path.relpath(fullpath, location)
        resname = bytes("/" + rel, 'utf-8')
        crc = hex(zlib.crc32(resname) & 0xffffffff)
        ## mimetype deduction
        ext = os.path.splitext(fullpath)
        if ext == '.css':
            mimetype = "text/css"
        elif ext == '.woff2':
            mimetype = "font/woff2"
        else:
            mimetype = mime.guess_type(name)[0]
            if mimetype is None:
                mimetype = "text/plain"
        print(rel, crc, mimetype)
        ## create switch case
        cases.append('  case ' + crc + ': {\n')
        cases.append('    const char ctype[] = "' + mimetype + '";\n')
        cases.append('    backend_response(200, ctype, sizeof(ctype)-1, ' + bname + ', ' + bname + '_size);\n')
        cases.append('  } break;\n')

file.write('unsigned static_site_count() {\n')
file.write('  return ' + str(len(binaries)) + ';\n')
file.write('}\n')
file.write('\n')

for bin in binaries:
    file.write(bin)
file.write('\n')

file.write('void static_site(const char *arg)\n')
file.write('{\n')
file.write('  uint32_t crc = crc32(arg);\n')
file.write('restart:\n')
file.write('  switch (crc) {\n')
for case in cases:
    file.write(case)
file.write('  } /* switch(crc) */\n')
file.write('} /* static_site */\n')

file.close()
