COMPILER?=gcc

build:
	mkdir -p bin
	$(COMPILER) src/consumer.c -o bin/consumer.out -w
	$(COMPILER) src/producer.c -o bin/producer.out -w