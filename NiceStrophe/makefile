#
# 'make depend' uses makedepend to automatically generate dependencies 
#               (dependencies are added to end of Makefile)
# 'make'        build executable file 'mycc'
# 'make clean'  removes all .d .o and executable files
#

#dir where src files are
PWD = ./src
# define the C compiler to use
CC = gcc

# define any compile-time flags
CFLAGS = -O0 -g3 -Wall -c -fmessage-length=0 -pthread

# define any directories containing header files other than /usr/include
#
INCLUDES_MAC = -I/usr/local/include -I/usr/local/include/nice -I/usr/local/opt/readline/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include/

INCLUDES_LNX = -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/local/include -I/usr/local/include/nice -I/usr/include/readline

INCLUDES = -I/usr/local/include -I/usr/local/include/nice -I/usr/local/opt/readline/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include/ -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/readline

# define library paths in addition to /usr/lib
#   if I wanted to include libraries not in /usr/lib I'd specify
#   their path using -Lpath, something like:
LFLAGS_MAC = -L$(PWD)/ -L/usr/local/opt/readline/lib -L/usr/local/lib/

LFLAGS_LNX = -L$(PWD)/ -L/usr/local/lib64

LFLAGS = -L$(PWD)/ -L/usr/local/lib64 -L/usr/local/opt/readline/lib -L/usr/local/lib

# define any libraries to link into executable:
#   if I want to link in libraries (libx.so or libx.a) I use the -llibname 
#   option, something like (this will link in libmylib.so and libm.so:
LIBS = -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lstrophe -lnice -llua -lm -lreadline 

# define the C source files
SRCS = $(PWD)/cmd.c $(PWD)/cmds.c $(PWD)/io.c $(PWD)/li.c $(PWD)/main.c $(PWD)/msg.c $(PWD)/net.c $(PWD)/nice.c $(PWD)/nice_action.c $(PWD)/roster.c \
	$(PWD)/thread_handler.c $(PWD)/tools.c $(PWD)/util.c

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = NiceStrophe

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

all:    $(MAIN)
	@echo  Simple compiler named NiceStrophe has been compiled

$(MAIN): $(OBJS) 
#$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)
	$(CC) $(LFLAGS) -o $(MAIN) $(OBJS)  $(LIBS)
# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<


clean:
	$(RM) $(PWD)/*.o $(PWD)/*.d $(PWD)/*~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it