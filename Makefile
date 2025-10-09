CC = gcc
serverSRC = server.c
clientSRC = client.c
serverOUT = server
clientOUT = client
all: $(serverOUT) $(clientOUT)
$(serverOUT): $(serverSRC)
	@$(CC) $(serverSRC) -o $(serverOUT)
$(clientOUT): $(clientSRC)
	@$(CC) $(clientSRC) -o $(clientOUT)
server_run: clean $(serverOUT)
	@./$(serverOUT)
client_run: clean $(clientOUT)
	@./$(clientOUT)
clean:
	@rm -f $(serverOUT) $(clientOUT)
