#GCC Makefile originally from Buzz
CC = g++
LD = g++

# Add the following compiler flags for your particular platform
# -m64 = 64-bits compile
# -m32 = 32-bits compile
# -static = static linkage
CFLAGSOLD =  -lm -g -x c -DNDEBUG -D_POSIX -O3 -w -static 
CFLAGS =  -lm -O3 -DNDEBUG -D_POSIX -O3 -w -static -march=corei7
# --profile-use

#Debugging Compiler-flags
#CFLAGS = -g -x c -DNDEBUG -D_POSIX -O0 -w -static

# Add the following compiler flags for your particular platform
# -m64 = 64-bits compile
# -m32 = 32-bits compile
# -static = static linkage
# -lm = math library
LDFLAGS =  -lm -s -pthread -O3
## -march=core2
# --profile-use

#Debugging Linker Flags
#LDFLAGS = -pthread -static -g

# use this command to erase files.
RM = /bin/rm -f
# list of generated object files.
OBJS =      attack.o \
     board.o \
     book.o \
     eval.o \
     fen.o \
     hash.o \
     list.o \
     main.o \
     material.o \
     move.o \
     move_check.o \
     move_do.o \
     move_evasion.o \
     move_gen.o \
     move_legal.o \
     option.o \
     pawn.o \
     piece.o \
     posix.o \
     protocol.o \
     pst.o \
     pv.o \
     random.o \
     recog.o \
     search.o \
     search_full.o \
     see.o \
     sort.o \
     square.o \
     trans.o \
     util.o \
     value.o \
     vector.o 




#gcc-profile-make (geht nicht mit flto zusammen):
#	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) 
	EXTRACXXFLAGS=-fprofile-generate 
	EXTRALDFLAGS='-lgcov' 


#gcc-profile-use:
#	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) 
#	EXTRACXXFLAGS=-fprofile-use -fno-peel-loops -fno-tracer 
#	EXTRALDFLAGS='-lgcov' 
## --profile-use

CFLAGS += -flto
LDFLAGS  += -flto 


# program executable file name.
PROG = togaii

$(PROG): $(OBJS) 
	$(LD) $(LDFLAGS) $(OBJS)  $(EXTRALDFLAGS)  -o $(PROG)

# meta-rule for compiling source files.
%.o: %.cpp
	$(CC) $(CFLAGS)  $(EXTRACXXFLAGS)  -c $<

clean:
	-rm *.o  
#*.gcda
##*~
