OBJECTS = DomainMG.o GridMG.o  PoissonMG.o

include ../../CONFIG

CFLAGS =  $(COMMONCFLAGS)  $(GTKCFLAGS) $(OGLCFLAGS)

all: compiling

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

compiling: $(OBJECTS)

clean:
	@$(RM) *.chk p *o gabedit *.com *.out *.log *.test test* *.cpp  *old *.ps .*swp *~ 

#make dependance
SRCS = $(OBJECTS:%.o=%.c)
dep :
	gcc -MM $(SRCS) > Dep.mk

-include Dep.mk

