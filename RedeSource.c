#include <sys/types.h>   // Definicao de tipos
#include <sys/socket.h>  // Biblioteca de estrutara de sockets
#include <netinet/in.h>  // Define os padroes de protocolo IP
#include <arpa/inet.h>   // Converte enderecos hosts
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Define constantes POSIX

typedef struct connection_node{ //Struct de conexão
	char address[512];
	char username[64];
	char online;
	char counter;
	struct connection_node* next;
} connection;

typedef struct { //Struct lista
	int size;
	connection* first;
} connection_list;

typedef struct message_list{
	char incoming;
	char address[64];
	char message[1024];
	struct message_list* next;
} conversation;

typedef struct {
	conversation* first;
} conversation_list;

connection_list ContactList;
conversation_list MessageList;
char* thisUsername;
char quit, justadd;
int removedTab;
pthread_mutex_t receivedMutex, pingMutex, sendMutex;
char receivedInfo[2][1024];
char* globalActive;

void sendMessage(char* address, char* message, int control);
void addAddress(char* address, int control);
void addContact(char* address, char* username);
void addContactRemote(char* address, char* username);
void removeContact(char* username);
void removeContactRemote(char* address);
connection* searchContact(char* address);
void groupMessage(char*);
int isEmpty();
void setGlobalActive(char* address);
void printContactList(void);
void saveContacts(void);
void loadContacts(void);
void logMsg(char* Content);
void saveListMsg(int incoming, char* address, char* message);
void printListMsg(char* address);
void printest(char* address);
void init(void);
void end(void);
void parseReceived(char* address, char* message);
int parseMessage(char* message);


void* messageReceiverThread(void){	
	char information[2][1024];
	strcpy(information[0],receivedInfo[0]);
	strcpy(information[1],receivedInfo[1]);
	parseReceived(information[0],information[1]);
	pthread_mutex_unlock(&receivedMutex);
}

void* pingThread(void){	
	while(!quit)
	{
		pthread_mutex_lock(&pingMutex);
		connection* iterator = ContactList.first;
		//logMsg("something1");
		while(iterator!=NULL){
			//logMsg("something2");
			if(iterator->counter==0){
				iterator->online=0;
			}
			//logMsg("tentou - ");
			struct sockaddr_in si_other;
			int s, i, slen=sizeof(si_other);
			struct hostent *host;
			host=gethostbyname(iterator->address);
			if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
				logMsg("Erro de socket");		
			}
			memset((char*) &si_other, 0, sizeof(si_other));
			si_other.sin_family=AF_INET;
			si_other.sin_port=htons(57123);
			si_other.sin_addr = *((struct in_addr *)host->h_addr);
			if(sendto(s,":ok", 3, 0, (struct sockaddr *) &si_other, slen)==-1)
			{
				logMsg("Erro no envio UDP");
			}
			//logMsg("enviar\n");
			close(s);	
			if(iterator->counter!=0)		
				iterator->counter=iterator->counter-1;
			iterator=iterator->next;
	
		}
		pthread_mutex_unlock(&pingMutex);
		sleep(3);
	}
}

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
	address.sin_port = htons(7123);
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
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("\33[H\33[2J");
	printf("##################################################################");
	printf("\n#\n# Servidor TCP esperando por conexoes na porta 7123\n#\n");
	printf("##################################################################");
	pthread_t PopupThread; //Popup Thread que o Gyogyo-san queria tanto

	while(!quit){

		connection_id = accept(socket_id, (struct sockaddr *)&incoming_address, &address_size);

		if(connection_id < 0) {
			logMsg("Erro no id de recebimento");
			exit(0);
		}
		//printf("\nMensagem Recebida");
		bytes_received=recv(connection_id,message,1024,0);
      		message[bytes_received] = '\0';
		
		pthread_mutex_lock(&receivedMutex);
		strcpy(receivedInfo[0],inet_ntoa(incoming_address.sin_addr));
		strcpy(receivedInfo[1],message);

		if (pthread_create(&PopupThread,0,(void*) messageReceiverThread,(void*) 0) != 0) { 
			printf("Error creating multithread.\n");
			exit(0);
		}
		
		
		close(connection_id);

		sleep(1);

	}
}

void* pingReceiverThread(void){

	int socket_id, true = 1;	struct sockaddr_in address;
	struct sockaddr_in incoming_address;	int address_size = sizeof(struct sockaddr_in);
	int connection_id;

	char message[1024];
	int bytes_received;

	if ((socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		logMsg("Erro no Socket");
		exit(0);
	}

	memset((char *) &address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_port = htons(57123);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(socket_id, (struct sockaddr *) &address, sizeof(address))==-1){
		logMsg("Erro no bind do socket UDP");
		exit(1);
	
	}
	//logMsg("bindiu");
	while(!quit){
		if(recvfrom(socket_id, message, 1024, 0, (struct sockaddr *) &incoming_address, &address_size)==-1){
			logMsg("Erro de recepcao");
			exit(0);		
		}
		//logMsg("recebeusmgthng");
		bytes_received=strlen(message);//(connection_id,message,1024,0);
      		message[bytes_received] = '\0';
		parseReceived(inet_ntoa(incoming_address.sin_addr),message);
	}

}

void* messengerThread(void){
	connection* activeContact = ContactList.first;
	char buffer[1024];
	int messagetype;
	char** groupmessage;
	sleep(3);
	while(!quit){
		if(justadd&&activeContact == NULL){
			setvbuf(stdout, NULL, _IONBF, 0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#");
			printf("\n# Aguarde\n#\n");
			printf("##################################################################");
			sleep(3);
			justadd=0;
			activeContact = ContactList.first;
		}
		else if(activeContact == NULL){
			setvbuf(stdout, NULL, _IONBF, 0);
			printf("\33[H\33[2J");
			printf("##################################################################");
			printf("\n#\n# Entre com qualquer linha para continuar.\n? ");
			getc(stdin);
			activeContact = ContactList.first;
		}
		setvbuf(stdout, NULL, _IONBF, 0);
		printf("\33[H\33[2J");
		if(activeContact != NULL){
			printf("##################################################################");
			printf("\n# Conversando com %s:",activeContact->username);
			printListMsg(activeContact->address);
			//printest(activeContact->address);
			printf("\n? Mensagem ou Comando: ");
		}		
		else{
			//printf("\33[H\33[2J");
			printf("##################################################################\n#\n");
			printf("# Nao ha contatos adicionados\n# Adicione um contato utilizando :a IP\n# Comando: ");
		}
		//printf("here0");
		//fflush(stdin);
		if(activeContact!=NULL) setGlobalActive(activeContact->address);
		__fpurge(stdin);
		fgets(buffer,956,stdin);
		messagetype = parseMessage(buffer);
		char buf[50];
		sprintf(buf,"removed tab %d", removedTab);
		logMsg(buf);
		if(removedTab){
			logMsg("erapraserdeletado");
			setvbuf(stdout, NULL, _IONBF,0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n#");
			printf(" Usuario deletado!\n#\n");
			printf("##################################################################");
			sleep(1);
			removedTab=0;
			messagetype=7;
			activeContact = ContactList.first;
		}	
		switch(messagetype){

			case 4: //"Tab":
			if(activeContact != NULL && activeContact->next != NULL) activeContact = activeContact->next;
			else activeContact = ContactList.first;
			break;

			case 5: //"Ta2":
			if(activeContact != NULL){
				connection* Marker = activeContact;
				activeContact = ContactList.first;
				while(activeContact != NULL){
					if(strcmp(activeContact->username,buffer) == 0) break;
					activeContact = activeContact->next;
				}

				if(activeContact == NULL){
					activeContact = Marker;
					setvbuf(stdout, NULL, _IONBF,0);
					printf("\33[H\33[2J");
					printf("##################################################################\n#\n#");
					printf(" O contato especificado nao existe!\n#\n");
					printf("##################################################################");
					sleep(1);
				}

			}
			else{
				setvbuf(stdout, NULL, _IONBF,0);
				printf("\33[H\33[2J");
				printf("##################################################################\n#\n#");
				printf(" Nao ha contatos adicionados!\n#\n");
				printf("##################################################################");
				sleep(1);
			}
			break;

			case 3: //"Exi":
			setvbuf(stdout, NULL, _IONBF, 0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n");
			printf("# Terminando o programa!\n");
			printf("#\n##################################################################");			
			sleep(1);
			printf("\33[H\33[2J");
			quit = 1;
			break;

			case 0: //"Add":
			//printf("\nImprimindo buffer %s\n", buffer);
			addAddress(buffer,0);
			break;

			case 2: //"Rem":
			//printf("%s",buffer);
			//logMsg(buffer);
			if(activeContact != NULL && strcmp(activeContact->username,buffer) == 0) {
				if(activeContact->next != NULL) activeContact = activeContact->next;
				else if (strcmp(activeContact->address,ContactList.first->address) == 0) activeContact = NULL;
				else activeContact = ContactList.first;
				}
			//logMsg(buffer);
			removeContact(buffer);
			//logMsg(buffer);
			break;

			case -1: //"Msg":
			if(activeContact != NULL){
				if(activeContact->online == 1){
					sendMessage(activeContact->address,buffer,0);	
				}
				
				else{
					setvbuf(stdout, NULL, _IONBF,0);
					printf("\33[H\33[2J");
					printf("##################################################################\n#\n#");
					printf(" %s nao esta online!\n#\n", activeContact->username);
					printf("##################################################################");
					sleep(1);
				}
			}
			else{
				setvbuf(stdout, NULL, _IONBF,0);
				printf("\33[H\33[2J");
				printf("##################################################################\n#\n#");
				printf(" Nao ha contatos adicionados!\n#\n");
				printf("##################################################################");
				sleep(1);
			}
			break;

			case 7: //"Frsh":
			//activeContact = ContactList.first;
			setvbuf(stdout, NULL, _IONBF,0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n#");
			printf(" Atualizando!\n#\n");
			printf("##################################################################");
			//sleep(0.5);
			break;

			case 8: //"List":

			if(activeContact != NULL){				
				printContactList();
			}
			else{
				setvbuf(stdout, NULL, _IONBF,0);
				printf("\33[H\33[2J");
				printf("##################################################################\n#\n#");
				printf(" Nao ha contatos adicionados!\n#\n");
				printf("##################################################################");
				sleep(1);
			}

			break;

			case 6: //"Grp":
			//printf("\n%s\n",buffer);
			groupMessage(buffer);
			break;

			case 1: //"Hlp":
			setvbuf(stdout, NULL, _IONBF,0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n#");
			printf("# Comandos do messenger:\n# :help (:h) - Exibe esta mensagem de ajuda\n# :fresh (:f) - Atualiza a conversa atual\n# :add <address> (:a) - Adiciona um contato pelo seu endereco IP\n# :remove <username> (:r) - Remove um contato adicionado\n# :quit (:q) - Sai do messenger\n# :tab <username> (:t) - Itera pelos contatos salvos, username for vazio, itera ao proximo\n# :group @<username1> @<username2> ... <mensagem>  (:g) - Mensagem em grupo para as pessoas da lista");
			printf("\n#\n##################################################################\n#\n#");
			printf(" Entre com qualquer linha para retornar ao modo de comando.\n? ");
			getc(stdin);
			break;

			default:
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n#");
			printf(" Comando nao identificado.\n# Digite :help para informacoes\n#");
			printf("\n#\n##################################################################");
			printf("\n#\n# Entre com qualquer linha para retornar ao modo de comando.\n? ");
			getc(stdin);

		}
		//printf("nmb %d ", messagetype); sleep(2);
	}
}

void sendMessage(char* address, char* message, int control){
	pthread_mutex_lock(&sendMutex);
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
	server_address.sin_port = htons(7123);
	server_address.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_address.sin_zero),8);
	
	if (connect(socket_id,(struct sockaddr *)&server_address,sizeof(struct sockaddr)) == -1){
		logMsg("Erro de conexao");
		printf("\33[H\33[2J");
		printf("Erro: %s\n", strerror(errno));
		close(socket_id);
		sleep(3);
	}
	else{
		if(!control){
			printf("\33[H\33[2J");
			if(message[0] == ':' && message[1] == 'a'){
				printf("##################################################################\n#\n#");
				printf(" Adicionando %s\n", address);	
				printf("#\n##################################################################");
				if(isEmpty()) justadd=1;
					
			}
			else if(message[0] == ':' && message[1] == 'r'){
				printf("##################################################################\n#\n#");
				printf(" Removendo %s\n", address);	
				printf("#\n##################################################################");
					
			}
		}
		send(socket_id,message,strlen(message),0);
		if(message[0]!=':') saveListMsg(0,address,message);
		close(socket_id);
		if(!control && (message[0] == ':' && message[1] == 'a' || message[0] == ':' && message[1] == 'r')) sleep(3);
		//sleep(3);
	}
	pthread_mutex_unlock(&sendMutex);
}

void addAddress(char* address, int control){
	//printf("here");
	char addMessage[1024];
	strcpy(addMessage,":a ");
	strcat(addMessage,thisUsername);
	//printf("addMessage %s!", addMessage);
	sendMessage(address,addMessage,control);
}

void addContact(char* address, char* username){
	pthread_mutex_lock(&pingMutex);
	connection* iterator = searchContact(address);
	if(iterator==NULL){
		iterator = ContactList.first;
		connection* newConnection = malloc(sizeof(connection));
		strcpy(newConnection->address,address);
		strcpy(newConnection->username,username);
		newConnection->online=1;
		newConnection->counter=3;
		newConnection->next = NULL;

		if(iterator == NULL) ContactList.first = newConnection;
		else {
			while(iterator->next != NULL) iterator = iterator->next;
			iterator->next = newConnection;
		}

		ContactList.size = ContactList.size + 1;
	}
	else
	{
		iterator->online=1;
		iterator->counter=3;
		strcpy(iterator->username,username);		
	}		
	pthread_mutex_unlock(&pingMutex);
}

void addContactRemote(char* address, char* username){
	pthread_mutex_lock(&pingMutex);
	connection* iterator = searchContact(address);
	if(iterator==NULL){
		iterator = ContactList.first;
		connection* newConnection = malloc(sizeof(connection));
		strcpy(newConnection->address,address);
		strcpy(newConnection->username,username);
		newConnection->online=1;
		newConnection->next = NULL;

		if(iterator == NULL) ContactList.first = newConnection;
		else {
			while(iterator->next != NULL) iterator = iterator->next;
			iterator->next = newConnection;
		}

		ContactList.size = ContactList.size + 1;
	}
	else
	{
		iterator->online=1;
		iterator->counter=3;
		strcpy(iterator->username,username);		
	}
	pthread_mutex_unlock(&pingMutex);
	char addMessage[1024];
	strcpy(addMessage,":k ");
	strcat(addMessage,thisUsername);
	//printf("addMessage %s!", addMessage);
	sendMessage(address,addMessage,1);
}

void removeContact(char* username){
	pthread_mutex_lock(&pingMutex);
	connection* iterator = ContactList.first;
	connection* iterator2;

	if(iterator == NULL) {}//Se lista vazia

	else if(strcmp(iterator->username,username) == 0) { //Se remover primeiro da lista

		if (ContactList.size == 1){
			ContactList.first = NULL;
			ContactList.size = 0;
		}

		else{
			ContactList.first = iterator->next;
			ContactList.size = ContactList.size - 1;
		}

		sendMessage(iterator->address,":r",0);
		free(iterator);
	}

	else { //Iterar lista
		iterator2 = iterator->next;
		while(iterator2 != NULL) {
			if(strcmp(iterator2->username,username) == 0){ //Se esse foi o escolhido
				iterator->next = iterator2->next;
				sendMessage(iterator2->address,":r",0);
				free(iterator2);
				ContactList.size = ContactList.size - 1;
				
			}
			iterator = iterator2;
			iterator2 = iterator->next;
		}
	}

	pthread_mutex_unlock(&pingMutex);
	return;
}

connection* searchContact(char* address){
	connection* iterator = ContactList.first;
	if(iterator == NULL) return;
	else if(strcmp(iterator->address,address) == 0) {
		return iterator;
	}
	else {
		while((iterator->next != NULL)) {
			if(strcmp(iterator->next->address,address) == 0)
				return iterator->next;
			iterator=iterator->next;
		}
	}
	return NULL;
}

void setGlobalActive(char* address){
	globalActive = address;
}

int isEmpty(){
	connection* iterator = ContactList.first;
	if(iterator == NULL) return 1;
	else	return 0;
}

void removeContactRemote(char* address){
	pthread_mutex_lock(&pingMutex);

	connection* iterator = ContactList.first;
	connection* iterator2;

	if(iterator == NULL) {} //Se lista vazia

	else if(strcmp(iterator->address,address) == 0) { //Se primeiro da lista
		
		if (ContactList.size == 1){
			ContactList.first = NULL;
			ContactList.size = 0;
		}

		else{
			ContactList.first = iterator->next;
			ContactList.size = ContactList.size - 1;
		}

		if(strcmp(iterator->address,globalActive)==0) removedTab=1;

		free(iterator);
	}

	else { //Iterar lista
		iterator2 = iterator->next;
		while(iterator2 != NULL) {
			if(strcmp(iterator2->address,address) == 0){ //Se esse foi o escolhido
				if(strcmp(iterator2->address,globalActive)==0) removedTab=1;
				iterator->next = iterator2->next;
				free(iterator2);
				ContactList.size = ContactList.size - 1;
			
			}
			iterator = iterator2;
			iterator2 = iterator->next;
		}
	}
	pthread_mutex_unlock(&pingMutex);
	return;
}

void parseReceived(char* address, char* message){

	if(message[0] == ':'){
		char* Sep = strchr(message,' ');
		if(Sep == NULL) Sep = strrchr(message,'\0');
		char ParseCode[1024];
		strncpy(ParseCode,message,(Sep - message + 1));
		ParseCode[(Sep - message + 1)] = '\0';
		//printf("a mensagem recebida foi parseada ParseCOde %s Message %s\n\n", ParseCode, message);
		if(strstr(ParseCode,":a")){
			//printf(" adicionando alguma coisa ");
			char* Separator = strchr(message,' ');
			logMsg("Added contact ");
			logMsg(address);
			addContactRemote(address,Separator+1);			
			//printf("tentou adicionar %s %s", address, Separator+1);
		}

		else if(strstr(ParseCode,":k")){
			//printf(" adicionando alguma coisa ");
			char* Separator = strchr(message,' ');
			logMsg("Added contact ");
			logMsg(address);
			addContact(address,Separator+1);			
			//printf("tentou adicionar %s %s", address, Separator+1);
		}

		else if(strstr(ParseCode,":r")){
			logMsg("Removed contact ");
			logMsg(address);
			removeContactRemote(address);
		}
		else if(strstr(ParseCode,":o")){
			//logMsg("chegou a mamae e o papai");
			connection* user = searchContact(address); 
			if(user==NULL)
				addAddress(address,1);
			else if(user->online==0){			
				user->online=1;
				user->counter=3;
			}
			else{
				user->counter=3;
			}
		}
	}

	else {
		saveListMsg(1,address,message);
		//printf("address %s message %s", address,message);
	}
}

int parseMessage(char* message){
	//char ParseMessage[1024] = strncpy (*message,ParseMessage,sizeof(*message));
	int returnvalue;

	if(message[0] == ':'){
		//printf("\nImprimindo message %s\n", message); sleep(3);
		char* Separator = strchr(message,' ');
		if(Separator == NULL) Separator = strrchr(message,'\n');
		char ParseCode[1024];
		strncpy(ParseCode,message,(Separator - message + 1));
		ParseCode[(Separator - message + 1)] = '\0';
		//printf("|%s|%s|%s|%d|",ParseCode,Separator,Separator+1,strlen(Separator+1));

		if(strstr(ParseCode,":a")){
			returnvalue = 0;
			//Código para copiar do separador ao final da string no buffer original para uso no messengerthread
			char* Separator2 = strrchr(message,'\n');
			Separator2[0] = '\0';
			if(Separator != Separator2) {
				//printf("\nPrintando Separator+1 %s e message %d\n", (Separator+1),(Separator2-Separator-2));
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
				//memmove(aux,(Separator+1),(Separator2-Separator-2));
				//memmove(message,aux,1024);
				//printf("|%s|%d|%s|%d", aux,strlen(message),message, strcmp(message,"123.123.123.123"));
				//sleep(20);
				//printf("\nMarkmark\n"); sleep(3);
			}
			else {logMsg("\nErro esquisito no Add\n");}
		}


		else if(strstr(ParseCode,":h")) returnvalue = 1;

		else if(strstr(ParseCode,":r")){
			returnvalue = 2;
			//printf("here1"); sleep(1);
			//__fpurge(stdout);
			char* Separator2 = strrchr(message,'\n');
			*Separator2 = '\0';
			if(Separator != Separator2) {
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
				returnvalue = 2;
				//printf("%s %d",message,strcmp(message,"username")); sleep(2);
			}
			//char buffer[50];
			//sprintf(buffer,"%d",returnvalue);
			//logMsg(buffer);
		}

		else if(strstr(ParseCode,":q")) returnvalue = 3;

		else if(strstr(ParseCode,":t")){
			returnvalue = 4;
			//printf("here");	
			//__fpurge(stdout);
			char* Separator2 = strrchr(message,' ');
			if(Separator2 != NULL) {
				Separator = strrchr(message,'\n');
				*Separator = '\0';
				char aux[1024];
				strcpy(aux,Separator2+1);
				strcpy(message,aux);
				returnvalue = 5;
			}
		}

		else if(strstr(ParseCode,":g")) {
			returnvalue = 6;
			char* Separator2 = strrchr(message,'\n');
			//*Separator2 = '\0';
			if(Separator != Separator2) {
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
			}
		}

		else if(strstr(ParseCode,":f")) returnvalue = 7;

		else if(strstr(ParseCode,":l")) returnvalue = 8;

		//char ReturnMessage[1024] = strncpy((Separator+1),ReturnMessage,sizeof(*message) - (Separator - *message + 1));
		//strcpy(message,strchr(message,' '));
		else{
			returnvalue=99;
		}
	}
	else{
		//printf("here");
		//char dataMessage[1024];
		//strcpy(dataMessage,thisUsername);
		//strcat(dataMessage," - ");
		//strcat(dataMessage,message);
		//strcpy(message,dataMessage);
		returnvalue = -1;
	}
	return returnvalue;
}

void groupMessage(char* buffer){
	char* identifier = buffer;
	int counter = 0;
	int i;

	while(identifier[0]!='\0'){
		if(identifier[0]=='@') counter++;
		identifier++;
	}

	char userSelector[64][64];

	if(!counter) return;

	char* identifier2 = buffer;
	identifier = buffer;
	i = 0;
	char setupFlag = 0;
	while(identifier[0]!='\0'){

		if(identifier[0]=='@'){
			identifier++;
			identifier2 = identifier;
			setupFlag = 1;
		}

		else if(identifier[0]==' ' && setupFlag == 1){
			identifier[0] = '\0';
			strcpy(userSelector[i],identifier2);
			identifier2 = identifier+1;
			while(identifier2[0]==' ') identifier2++;
			identifier = identifier2;
			i++;
			setupFlag = 0;
		}

		else identifier++;
	}
	//tam=strlen(buffer);
	//buffer[tam]='\n';
	//if(setupFlag == 1) Não teve mensagem pra enviar, dur.
	//else identifier2 terá a mensagem para enviar.
	if(!setupFlag) {

		strcpy(buffer,identifier2);
		pthread_mutex_lock(&pingMutex);
		connection* iterator = ContactList.first;

		if(iterator != NULL)
			for(i = 0; i < counter; i++){
				while(iterator != NULL && strcmp(iterator->username,userSelector[i])!=0)
					iterator = iterator->next;
				if(iterator != NULL){ //Encontrou username na lista de contatos

					if(iterator->online == 1){	
						sendMessage(iterator->address,buffer,0);
					}
				
					else{
						setvbuf(stdout, NULL, _IONBF,0);
						printf("\33[H\33[2J");
						printf("##################################################################\n#\n#");
						printf(" %s nao esta online!\n#\n", iterator->username);
						printf("##################################################################");
						sleep(1);
					}
				}
				else{
						setvbuf(stdout, NULL, _IONBF,0);
						printf("\33[H\33[2J");
						printf("##################################################################\n#\n#");
						printf(" %s nao existe na lista de contatos!\n#\n", userSelector[i]);
						printf("##################################################################");
						sleep(1);
				}
			}
		pthread_mutex_unlock(&pingMutex);
		
	}
	return;
}

void printContactList(void){
	setvbuf(stdout, NULL, _IONBF,0);
	printf("\33[H\33[2J");
	printf("##################################################################\n#\n#");
	printf(" Imprimindo lista de contatos:\n#");

	connection* iterator = ContactList.first;

	while(iterator != NULL){
		if(iterator->online)
			printf("\n#\n# Username: %s\n# Address:  %s\n# Online:   Sim",iterator->username,iterator->address); 
		else		
			printf("\n#\n# Username: %s\n# Address:  %s\n# Online:   Nao",iterator->username,iterator->address); 
		iterator = iterator->next;
	}

	printf("\n#\n# Entre com qualquer linha para continuar.\n");
	getc(stdin);
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
			addAddress(buffer,1);
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

void saveListMsg(int incoming, char* address, char* message){
	//printf(" salvando ");
	conversation* iterator = MessageList.first;
	conversation* newConversation= malloc(sizeof(conversation));
	newConversation->incoming=incoming;
	newConversation->next=NULL;
	strcpy(newConversation->address,address);
	strcpy(newConversation->message,message);	
	if(iterator == NULL){
		//printf(" eranulo ");
		MessageList.first = newConversation;
	}
	else{
		//printf(" neranulo ");
		while(iterator->next!=NULL)
			iterator=iterator->next;
		iterator->next = newConversation;
	}
	//printf(" terminei salvar "); sleep(3);
}

void printListMsg(char* address){
	conversation* iterator = MessageList.first;
	connection* user = searchContact(address);
	if(iterator == NULL || address == NULL){
		printf("\n#");
		return;
	}
	else{
		printf("\n#\n");
		if(strcmp(iterator->address,address)==0)
		{
			//printf("1st %d ", iterator->incoming);
			if(!iterator->incoming) printf("#\n# %s: %s", thisUsername,iterator->message);
			else if(iterator->incoming) printf("#\n# %s: %s", user->username, iterator->message);
		}
		while(iterator->next!=NULL)
		{
			iterator=iterator->next;
			if(strcmp(iterator->address,address)==0)
			{
				if(!iterator->incoming) printf("#\n# %s: %s", thisUsername, iterator->message);
				else if(iterator->incoming) printf("#\n# %s: %s", user->username, iterator->message);
			}
		}
		printf("#");
	}
	//printf(" terminei printar "); sleep(3);
}

void printest(char* address){
	conversation* iterator = MessageList.first;
	if(iterator == NULL){
		//printf("nemfoi");
		return;
	}
	else{
		printf("\n");
		printf("\n%s: %s", address, iterator->message);
		while(iterator->next!=NULL)
		{
			iterator=iterator->next;
			printf("\n%s: %s", address, iterator->message);
		}
	}
	//printf(" terminei printar "); sleep(3);
}

void init(void){
	thisUsername = (char*) malloc (64*sizeof(char));
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("\33[H\33[2J");
	printf("##################################################################\n#\n#");
	printf(" Bem vindo ao messenger!\n# Por favor digite seu nome de usuario.\n# Username: ");
	__fpurge(stdin);
	fgets(thisUsername,63,stdin);
	char* ReturnOfNewlineKiller = strrchr(thisUsername,'\n');
	*ReturnOfNewlineKiller = '\0';
	quit = 0;
	justadd = 0;
	removedTab = 0;
	//ContactList.first = malloc(sizeof(connection));
	//strcpy(ContactList.first->address,"localhost");
	//strcpy(ContactList.first->username,thisUsername);
	ContactList.first = NULL;
	MessageList.first = NULL;
	if(pthread_mutex_init(&receivedMutex,NULL) != 0){
		printf("Error creating Mutex.\n");
		exit(0);
	}
	if(pthread_mutex_init(&pingMutex,NULL) != 0){
		printf("Error creating Mutex.\n");
		exit(0);
	}
	if(pthread_mutex_init(&sendMutex,NULL) != 0){
		printf("Error creating Mutex.\n");
		exit(0);
	}
}


void end(void){
	saveContacts();
	logMsg("End Of Execution");
	free(thisUsername);
	pthread_mutex_destroy(&receivedMutex);
	pthread_mutex_destroy(&pingMutex);
	pthread_mutex_destroy(&sendMutex);
	exit(0);
}
void main(void){

	pthread_t ReceiverThread;
	pthread_t MessengerThread;
	pthread_t PingThread;
	pthread_t PingReceiverThread;

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

	if (pthread_create(&PingThread,0,(void*) pingThread,(void*) 0) != 0) { 
		printf("Error creating multithread.\n");
		exit(0);
	}

	if (pthread_create(&PingReceiverThread,0,(void*) pingReceiverThread,(void*) 0) != 0) { 
		printf("Error creating multithread.\n");
		exit(0);
	}

	while(!quit){}

	end();
}

