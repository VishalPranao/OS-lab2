PAIRS ?= 3

.PHONY: default clean run

default:
	$(MAKE) -C makeprocs
	$(MAKE) -C producer
	$(MAKE) -C consumer

clean:
	$(MAKE) -C makeprocs clean
	$(MAKE) -C producer clean
	$(MAKE) -C consumer clean

run:
	cd ../../bin && dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj $(PAIRS)
