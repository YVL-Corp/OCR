train :
	gcc main_letters.c neural_network.c image_loader.c network_io.c -o letter_trainer `pkg-config --cflags --libs gtk+-3.0` -lm

clean :
	rm -f output letter_trainer
