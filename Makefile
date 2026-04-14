build:
	gcc process_generator.c clk_utils.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc scheduler.c clk_utils.c -o scheduler.out
	gcc process.c clk_utils.c -o process.out
	gcc test_generator.c -o test_generator.out

clean:
	rm -f *.out  processes.txt

all: clean build

run:
	./process_generator.out processes.txt -sch 3 -q 2
