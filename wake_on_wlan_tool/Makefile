########################################################
#  MakeFile for RTL871X_TOOL.                          #
#                                                      #
########################################################


CC      = gcc
CCFLAGS = -Wall
#BUILDFLAGS = -static
#LIB     = -lpthread
LIB     = -pthread

object  = wowlan_ctrl.o
output  = wowlan


GENOBJ = $(CC) $(CCFLAGS) -c


.KEEP_STATE:

all: build

build: $(object)
	$(CC) $(CCFLAGS) $(BUILDFLAGS) $(LIB) $(object) -o $(output)

# =============== object generate =======================

wowlan_ctrl.o: wowlan_ctrl.c


# =============== object generate =======================

clean:
	rm -f *.o *~ $(output)
