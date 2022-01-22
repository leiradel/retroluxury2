ifneq ($(findstring Linux,$(shell uname -a)),)
    SOEXT=so
else ifneq ($(findstring Darwin,$(shell uname -a)),)
    SOEXT=dylib
else
    SOEXT=dll
endif

%.o: %.c
	@echo "Compiling: $@"
	@$(CC) $(CFLAGS) -c "$<" -o "$@"

CC ?= gcc
CFLAGS = -std=c99 -Wall -Wpedantic -Werror -fPIC
DEFINES += -DWITH_MEM_SRCDST=0 # libjpeg-turbo

INCLUDES += -Isrc/engine -Isrc/generated
INCLUDES += -Isrc/libjpeg-turbo # libjpeg-turbo
INCLUDES += -Isrc/libpng -Isrc/zlib # libpng and zlib

LIBS = -lm

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g $(DEFINES) $(INCLUDES) -DRL2_BUILD_DEBUG
else
	CFLAGS += -O3 -DNDEBUG $(DEFINES) $(INCLUDES) -DRL2_BUILD_RELEASE
endif

ENGINE_OBJS = \
	src/engine/rl2_canvas.o src/engine/rl2_djb2.o src/engine/rl2_filesys.o src/engine/rl2_heap.o src/engine/rl2_log.o \
	src/engine/rl2_pixelsrc.o

LIBJPEG_TURBO_OBJS = \
	src/3rdparty/libjpeg-turbo/jaricom.o src/3rdparty/libjpeg-turbo/jcomapi.o src/3rdparty/libjpeg-turbo/jdapimin.o \
	src/3rdparty/libjpeg-turbo/jdapistd.o src/3rdparty/libjpeg-turbo/jdarith.o src/3rdparty/libjpeg-turbo/jdcoefct.o \
	src/3rdparty/libjpeg-turbo/jdcolor.o src/3rdparty/libjpeg-turbo/jddctmgr.o src/3rdparty/libjpeg-turbo/jdhuff.o \
	src/3rdparty/libjpeg-turbo/jdinput.o src/3rdparty/libjpeg-turbo/jdmainct.o src/3rdparty/libjpeg-turbo/jdmarker.o \
	src/3rdparty/libjpeg-turbo/jdmaster.o src/3rdparty/libjpeg-turbo/jdmerge.o src/3rdparty/libjpeg-turbo/jdphuff.o \
	src/3rdparty/libjpeg-turbo/jdpostct.o src/3rdparty/libjpeg-turbo/jdsample.o src/3rdparty/libjpeg-turbo/jerror.o \
	src/3rdparty/libjpeg-turbo/jidctflt.o src/3rdparty/libjpeg-turbo/jidctfst.o src/3rdparty/libjpeg-turbo/jidctint.o \
	src/3rdparty/libjpeg-turbo/jidctred.o src/3rdparty/libjpeg-turbo/jmemmgr.o src/3rdparty/libjpeg-turbo/jmemnobs.o \
	src/3rdparty/libjpeg-turbo/jquant1.o src/3rdparty/libjpeg-turbo/jquant2.o src/3rdparty/libjpeg-turbo/jsimd_none.o \
	src/3rdparty/libjpeg-turbo/jutils.o

LIBPNG_OBJS = \
	src/3rdparty/libpng/pngerror.o src/3rdparty/libpng/pngget.o src/3rdparty/libpng/pngmem.o src/3rdparty/libpng/png.o \
	src/3rdparty/libpng/pngread.o src/3rdparty/libpng/pngrio.o src/3rdparty/libpng/pngrtran.o src/3rdparty/libpng/pngrutil.o \
	src/3rdparty/libpng/pngset.o src/3rdparty/libpng/pngtrans.o src/3rdparty/libpng/pngwio.o src/3rdparty/libpng/pngwrite.o \
	src/3rdparty/libpng/pngwtran.o src/3rdparty/libpng/pngwutil.o

ZLIB_OBJS = \
	src/3rdparty/zlib/adler32.o src/3rdparty/zlib/crc32.o src/3rdparty/zlib/deflate.o src/3rdparty/zlib/inffast.o \
	src/3rdparty/zlib/inflate.o src/3rdparty/zlib/inftrees.o src/3rdparty/zlib/trees.o src/3rdparty/zlib/zutil.o

RETROLUXURY2_OBJS = $(ENGINE_OBJS)
3RDPARTY_OBJS = $(LIBJPEG_TURBO_OBJS) $(LIBPNG_OBJS) $(ZLIB_OBJS)

all: libretroluxury2.a

libretroluxury2.a: $(RETROLUXURY2_OBJS) $(3RDPARTY_OBJS)
	ar rcs $@ $+

src/generated/version.h: FORCE
	@echo "Creating version header: $@"
	@cat etc/version.templ.h \
		| sed s/\&HASH/`git rev-parse HEAD | tr -d "\n"`/g \
		| sed s/\&VERSION/`git tag | sort -r -V | head -n1 | tr -d "\n"`/g \
		| sed s/\&DATE/`date -Iseconds`/g \
		> $@

clean: FORCE
	@echo "Cleaning up"
	@rm -f libretroluxury2.a $(RETROLUXURY2_OBJS)
	@rm -f src/generated/version.h src/runtime/bootstrap.lua.h $(PNG_HEADERS) $(LUA_HEADERS)

distclean: clean
	@echo "Cleaning up (including 3rd party libraries)"
	@rm -f $(3RDPARTY_OBJS)

.PHONY: FORCE