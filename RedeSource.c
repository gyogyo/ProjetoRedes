#include <sys/types.h>   // Definicao de tipos
#include <sys/socket.h>  // Biblioteca de estrutara de sockets
#include <netinet/in.h>  // Define os padroes de protocolo IP
#include <arpa/inet.h>   // Converte enderecos hosts
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Define constantes POSIX

typedef struct connection_node{
	char address[512];
	char username[64];
	struct connection_node* next;
} connection;

typedef struct {
	int size;
	connection* first;
} connection_list;

connection_list ContactList;
char* thisUsername;
char quit;

void sendMessage(char* address, char* message);
void addAddress(char* address);
void addContact(char* address, char* username);
void removeContact(char* username);
void removeContactRemote(char* address);
void saveContacts(void);
void loadContacts(void);
void logMsg(char* Content);
void init(void);
void end(void);
void parseReceived(char* address, char* message);
int parseMessage(char* message);


void* receiverThread(void){

	int socket_id, true = 1;	struct sockaddr_in address;
	struct sockaddr_in incoming_address;	int address_size = sizeof(struct sockaddr_in);
	int connection_id;

	char message[1024];
	int bytes_received;

	if ((socket_id = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		logMsg("Erro no Socket");
		exit(0);
	}

	if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int)) == -1) {
		logMsg("Erro Setsockopt");
		exit(0);
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(7000);
	address.sin_addr.s_addr = INADDR_ANY;
	bzero(&(address.sin_zero),8);

	if (bind(socket_id, (struct sockaddr *) &address, sizeof(struct sockaddr)) == -1) {
		logMsg("Nao foi possivel realizar o bind");
		exit(0);
	}

	if (listen(socket_id, 10) == -1) {
		logMsg("Erro de Listen");
		exit(0);
	}

	printf("\nServidor TCP esperando por conexoes na porta 7000\n");

	while(!quit){

		connection_id = accept(socket_id, (struct sockaddr *)&incoming_address, &address_size);

		if(connection_id < 0) {
			logMsg("Erro no id de recebimento");
			exit(0);
		}
		printf("\nMensagem Recebida");
		bytes_received=recv(connection_id,message,1024,0);
      		message[bytes_received] = '\0';

		parseReceived(inet_ntoa(incoming_address.sin_addr),message);
		
		close(connection_id);

		sleep(1);

	}
}

void* messengerThread(void){
	connection* activeContact = ContactList.first;
	char buffer[1024];
	int messagetype;
	int* groupmessage;
	while(!quit){
		if(activeContact != NULL){
			printf("\nConversando com %s:\n",activeContact->username);
		}		
		else{
			printf("\nNao ha contatos adicionados\nAdicione um contato utilizando \\a\n");
		}
		//printf("here0");
		fflush(stdin);
		__fpurge(stdin);
		fgets(buffer,956,stdin);
		//printf("here1");	
		//sleep(10);
		messagetype = parseMessage(buffer);
		//printf("here2");
		switch(messagetype){

			case 4: //"Tab":
			if(activeContact != NULL && activeContact->next != NULL) activeContact = activeContact->next;
			else activeContact = ContactList.first;
			break;

			case 5: //"Ta2":
			if(activeContact != NULL && activeContact->next != NULL){
				connection* Marker = activeContact;
				do {
					activeContact = activeContact->next;
				} while(activeContact->username != buffer && activeContact != Marker);
			}
			else
				printf("\nNao ha Contatos\n");
			break;

			case 3: //"Exi":
			quit = 1;
			break;

			case 0: //"Add":
			//printf("\nImprimindo buffer %s\n", buffer);
			addAddress(buffer);
			break;

			case 2: //"Rem":
			removeContact(buffer);
			break;

			case -1: //"Msg":
			if(activeContact != NULL) sendMessage(buffer,activeContact->address);
			break;
			/*
			case 6: //"Grp":
			groupmessage = groupSelect(buffer);
			if(!groupmessage[0]) printf("Grupo invalido\n");
			else for(i = 1; i <= groupmessage[0]; i++)
			break;*/

			case 1: //"Hlp":
			printf("Comandos do messenger:\n\\help (\\h) - Exibe esta mensagem de ajuda\n\\add <address> (\\a) - Adiciona um contato pelo seu endereco IP\n\\remove <username> (\\r) - Remove um contato adicionado\n\\quit (\\q) - Sai do messenger\n\\tab <username> (\\t) - Itera pelos contatos salvos, username for vazio, itera ao proximo\n\\group @<username1> @<username2> ... <mensagem>  (\\g) - Mensagem em grupo para as pessoas da lista\n");
			break;

			default:
			printf("Comando nao identificado. Digite \\help para informacoes\n");

		}
	}
}

void sendMessage(char* address, char* message){

	int socket_id;
	struct hostent* host;
	struct sockaddr_in server_address;
	int connection_id;
	//printf("\nImprimindo address %s e message %s\n", address, message);	
	//printf("%d", strlen(message));
	int i;
	/*for(i=0;i<10;i++)
		printf("[%c]", address[i]);*/
	int bytes_received;
	//printf("address %s. Number %d. name %s.", address,(int)strlen(address), message);
	host = gethostbyname(address);
	
	if ((socket_id = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		logMsg("Erro no Socket");
		exit(0);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(7000);
	server_address.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_address.sin_zero),8);

	if (connect(socket_id,(struct sockaddr *)&server_address,sizeof(struct sockaddr)) == -1){
		logMsg("Erro de conexao");
		exit(0);
	}

	printf("\nMandando mensagem para %s\n",address);

	send(socket_id,message,strlen(message),0);

	close(socket_id);
}

void addAddress(char* address){
	//printf("here");
	char addMessage[1024] = "\a ";
	strcat(addMessage,thisUsername);
	sendMessage(address,addMessage);
}

void addContact(char* address, char* username){
	connection* iterator = ContactList.first;

	connection* newConnection = malloc(sizeof(connection));
	strcpy(newConnection->address,address);
	strcpy(newConnection->username,username);
	newConnection->next = NULL;

	if(iterator == NULL) ContactList.first = newConnection;
	else {
		while(iterator->next != NULL) iterator = iterator->next;
		iterator->next = newConnection;
	}

	ContactList.size = ContactList.size + 1;

}

void removeContact(char* username){
	connection* iterator = ContactList.first;
	connection* iterator2;

	if(iterator == NULL) return;
	else if(!strcmp(iterator->username,username)) {
		ContactList.first = iterator->next;
		sendMessage(iterator->address,"\r");
		free(iterator);
		ContactList.size = ContactList.size - 1;
		return;
	}
	else {
		while((iterator->next != NULL) && (strcmp(iterator->next->username,username) == 0)) {
			iterator2 = iterator->next;
			iterator->next = iterator2->next;
			sendMessage(iterator2->address,"\r");
			free(iterator2);
			ContactList.size = ContactList.size - 1;
			return;
		}
	}

}

void removeContactRemote(char* address){
	connection* iterator = ContactList.first;
	connection* iterator2;

	if(!strcmp(iterator->address,address)) {
		ContactList.first = iterator->next;
		free(iterator);
		ContactList.size = ContactList.size - 1;
		return;
	}

	else {
		while((iterator->next != NULL) && (strcmp(iterator->next->address,address) == 0)) {
			iterator2 = iterator->next;
			iterator->next = iterator2->next;
			free(iterator2);
			ContactList.size = ContactList.size - 1;
			return;
		}
	}

}

void parseReceived(char* address, char* message){

	if(message[0] == '\\'){
		char* Sep = strchr(message,' ');
		if(Sep == NULL) Sep = strrchr(message,'\0');
		char ParseCode[1024];
		strncpy(ParseCode,message,(Sep - message + 1));
		ParseCode[(Sep - message + 1)] = '\0';
		if(strstr(ParseCode,"\\a")){
			char* Separator = strchr(message,' ');
			logMsg("Added contact ");
			logMsg(address);
			addContact(address,Separator+1);
		}

		else if(strstr(ParseCode,"\\r")){
			logMsg("Removed contact ");
			logMsg(address);
			removeContact(address);
		}
	}

	else {
		logMsg(message);
		//printf("%s",message);
	}

}

int parseMessage(char* message){
	//char ParseMessage[1024] = strncpy (*message,ParseMessage,sizeof(*message));
	int returnvalue;

	if(message[0] == '\\'){
		//printf("\nImprimindo message %s\n", message);
		char* Separator = strchr(message,' ');
		if(Separator == NULL) Separator = strrchr(message,'\0');
		char ParseCode[1024];
		strncpy(ParseCode,message,(Separator - message + 1));
		ParseCode[(Separator - message + 1)] = '\0';
		//printf("\nImprimindo message %s\n", message);

		if(strstr(ParseCode,"\\a")){
			returnvalue = 0;
			//Código para copiar do separador ao final da string no buffer original para uso no messengerthread
			char* Separator2 = strrchr(message,'\0');
			if(Separator != Separator2) {
				//printf("\nPrintando Separator+1 %s e message %d\n", (Separator+1),(Separator2-Separator-2));
				char aux[1024];
				memmove(aux,(Separator+1),(Separator2-Separator-2));
				memmove(message,aux,1024);
				//printf("%s", message);
				returnvalue = 0;
				//printf("here");
			}
		}

		else if(strstr(ParseCode,"\\h")) returnvalue = 1;

		else if(strstr(ParseCode,"\\r")){
			returnvalue = 2;
			char* Separator2 = strrchr(message,'\0');
			if(Separator != Separator2) {
				char aux[1024];
				memmove(aux,(Separator+1),(Separator2-Separator-2));
				memmove(message,aux,1024);
				returnvalue = 2;
			}
		}

		else if(strstr(ParseCode,"\\q")) returnvalue = 3;

		else if(strstr(ParseCode,"\\t")){
			returnvalue = 4;
			//printf("here");	
			//__fpurge(stdout);
			char* Separator2 = strrchr(message,' ');
			if( Separator2 != NULL) {
				Separator = strrchr(message,'\0');
				if (Separator != Separator2)
				{
					char aux[1024];
					memmove(aux,(Separator2+1),(Separator-Separator2-2));
					memmove(message,aux,1024);
					//printf("%s", message);
					message[(Separator - Separator2 - 2)] = '\0';
					returnvalue = 5;
				}
			}
		}

		else if(strstr(ParseCode,"\\g")) {
			returnvalue = 6;
			char* Separator2 = strrchr(message,'\0');
			if(Separator != Separator2) {
				char aux[1024];
				memmove(aux,(Separator+1),(Separator2-Separator-2));
				memmove(message,aux,1024);
				returnvalue = 6;
			}
		}

		//char ReturnMessage[1024] = strncpy((Separator+1),ReturnMessage,sizeof(*message) - (Separator - *message + 1));
		//strcpy(message,strchr(message,' '));
	}
	else
	{
		//printf("here");
		char dataMessage[1024];
		strcpy(dataMessage,thisUsername);
		strcat(dataMessage," - ");
		strcat(dataMessage,message);
		strcpy(message,dataMessage);
		returnvalue = -1;
	}
	//printf("retoronou");
	return returnvalue;

}


																																																																																																																																																																																																																																																																																																																								void saveContacts(void){
	FILE* SaveFile = fopen("ContactList.txt","w");
	connection* it = ContactList.first;
	//char buffer[512];
	if(SaveFile != NULL){

		while(it != NULL){
		//sprintf(buffer, "%s\n",it->address);
		fputs(it->address,SaveFile);
		fputc('\n',SaveFile);
		it = it->next;
		}

		fclose(SaveFile);
	}
}

void loadContacts(void){
	FILE* LoadFile = fopen("ContactList.txt","r");
	char buffer[512];
	if(LoadFile != NULL&&ftell(LoadFile)){
		while(fgets(buffer,16,LoadFile)){
			addAddress(buffer);
		}
		fclose(LoadFile);
	}
	if(!ftell(LoadFile)) fclose(LoadFile);
}

void logMsg(char* Content){
	FILE* LogFile = fopen("log.txt","a");
	fputs(Content,LogFile);
	fputc('\n',LogFile);
	fclose(LogFile);
}

void init(void){
	thisUsername = (char*) malloc (64*sizeof(char));
	printf("\nBem vindo ao messenger!\nPor favor digite seu nome de usuario:\n");
	__fpurge(stdin);
	fgets(thisUsername,63,stdin);
	quit = 0;
	//ContactList.first = malloc(sizeof(connection));
	//strcpy(ContactList.first->address,"localhost");
	//strcpy(ContactList.first->username,thisUsername);
	ContactList.first = NULL;
}


void end(void){
	saveContacts();
	logMsg("End Of Execution");
}
void main(void){

	pthread_t ReceiverThread;
	pthread_t MessengerThread;

	init();

	if (pthread_create(&ReceiverThread,0,(void*) receiverThread,(void*) 0) != 0) { 
		printf("Error creating multithread.\n");
		exit(0);
	}

	//loadContacts();

	if (pthread_create(&MessengerThread,0,(void*) messengerThread,(void*) 0) != 0) { 
		printf("Error creating multithread.\n");
		exit(0);
	}

	while(!quit){}

	end();
}

