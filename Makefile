all: clean memDump checksum getFaxData

debug: clean checksum-dbg getFaxData-dbg memDump-dbg

checksum:
	gcc -Wall src/modemChecksum.c -o bin/checksum

checksum-dbg:
	gcc -g -Wall src/modemChecksum.c -o bin/checksum

getFaxData-dbg:
	gcc -g -Wall src/getFaxData.c -o bin/getFaxData -lsqlite3

memDump-dbg:
	gcc -g -Wall src/memDump.c -o bin/memDump

getFaxData:
	gcc -Wall src/getFaxData.c -o bin/getFaxData -lsqlite3

memDump:
	gcc -Wall src/memDump.c -o bin/memDump

clean:
	rm -f bin/*
