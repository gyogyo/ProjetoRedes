/*
Trabalho de Redes - Messenger simples com uso direto e não abstraído de sockets.
Feito por:
Lucas Hilário Favaro Leal - 8503944
Gyordano Gadoni Reis - 85513algumacoisa
*/

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

typedef struct message_list{ //Struct log da conversa
	char incoming;
	char address[64];
	char message[1024];
	struct message_list* next;
} conversation;

typedef struct { //Struct lista da conversa
	conversation* first;
} conversation_list;

connection_list ContactList; //Lista de contatos global ao programa
conversation_list MessageList; //Lista de mensagens global ao programa
char* thisUsername; //String global do username do cliente
char quit, justadd; //Variáveis globais de controle diversos
int removedTab; //Variável global usada para controle de abas removidas
pthread_mutex_t receivedMutex, pingMutex, sendMutex; //Mutexes globais
char receivedInfo[2][1024]; //Informações recebidas para uso multithread
char* globalActive; //String global tendo o ip da janela atual

void sendMessage(char* address, char* message, int control); //send message to address
void addAddress(char* address, int control); //add IP address
void addContact(char* address, char* username); //add a new contact to list
void addContactRemote(char* address, char* username); //add a new contact to list (Receiver Thread)
void removeContact(char* username); //remove a contact from list
void removeContactRemote(char* address); //remove a contact from list (Receiver Thread)
connection* searchContact(char* address); //returns null if address not found in contacts
void groupMessage(char*); //sends a group message in the format @1 @2 @3 <Message>
int isEmpty(); //returns true if ContactList empty
void setGlobalActive(char* address); //setter for global variable globalActive
void printContactList(void); //prints the contact list on stdout
void saveContacts(void); //saves the contacts to an external file (Not Used)
void loadContacts(void); //loads the contacts from an external file (Not Used)
void logMsg(char* Content); //saves Content to the log file
void saveListMsg(int incoming, char* address, char* message); //
void printListMsg(char* address); //
void printest(char* address); //print test (Not Used)
void init(void); //program initializer function
void end(void); //program ender function
void parseReceived(char* address, char* message); //parses received messages from the Receiver Thread
int parseMessage(char* message); //parses user input from stdin


void* messageReceiverThread(void){ //Thread para deixar paralelo o tratamento de mensagens recebidas.

	char information[2][1024];

	strcpy(information[0],receivedInfo[0]); //Cópia local de variáveis globais.
	strcpy(information[1],receivedInfo[1]);
	parseReceived(information[0],information[1]);
	pthread_mutex_unlock(&receivedMutex); //Liberar mutex após uso.

}

void* pingThread(void){	
	while(!quit)
	{
		pthread_mutex_lock(&pingMutex); // impede que acessem a lista enquanto remove ou adiciona um contato
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
			if(sendto(s,":ok", 3, 0, (struct sockaddr *) &si_other, slen)==-1) // envia msg UDP de controle, ela confirma "estou online"
			{
				logMsg("Erro no envio UDP");
			}
			//logMsg("enviar\n");
			close(s);	
			if(iterator->counter!=0)		
				iterator->counter=iterator->counter-1; // counter eh utilizado para contar o tempo de timeout
			iterator=iterator->next; // ate que um usuario esteja offline, toda vez que envia um pacote UDP diminui em 1
	
		}
		pthread_mutex_unlock(&pingMutex);
		sleep(3);
	}
}

void* receiverThread(void){ //Thread para o recebimento de mensagens

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

	while(!quit){ //Enquanto o programa não sair e já está com o server na escuta

		connection_id = accept(socket_id, (struct sockaddr *)&incoming_address, &address_size);

		if(connection_id < 0) {
			logMsg("Erro no id de recebimento");
			exit(0);
		}
		//printf("\nMensagem Recebida");
		bytes_received=recv(connection_id,message,1024,0);
      		message[bytes_received] = '\0';
		
		pthread_mutex_lock(&receivedMutex); //Travar mutex para impedir um possível erro no tratamento.
		strcpy(receivedInfo[0],inet_ntoa(incoming_address.sin_addr)); //Salvar em variável global para multithread.
		strcpy(receivedInfo[1],message);

		if (pthread_create(&PopupThread,0,(void*) messageReceiverThread,(void*) 0) != 0) { 
			printf("Error creating multithread.\n");
			exit(0);
		}
		
		
		close(connection_id); //Fechar o socket aceito

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
	address.sin_port = htons(57123); // fica em escuta nessa porta ate o programa terminar
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(socket_id, (struct sockaddr *) &address, sizeof(address))==-1){
		logMsg("Erro no bind do socket UDP");
		exit(1);
	
	}
	//logMsg("bindiu");
	while(!quit){
		if(recvfrom(socket_id, message, 1024, 0, (struct sockaddr *) &incoming_address, &address_size)==-1){
			logMsg("Erro de recepcao");		
		}
		//logMsg("recebeusmgthng");
		bytes_received=strlen(message);//(connection_id,message,1024,0);
      		message[bytes_received] = '\0';
		parseReceived(inet_ntoa(incoming_address.sin_addr),message); //rotina de tratamento da mensagem
	}

}

void* messengerThread(void){ //Thread 'principal', cuida das entradas do usuário.
	connection* activeContact = ContactList.first;
	char buffer[1024];
	int messagetype;
	char** groupmessage;
	sleep(3);
	while(!quit){
		if(justadd&&activeContact == NULL){ //Se o primeiro contato foi adicionado, dar um tab automático.
			setvbuf(stdout, NULL, _IONBF, 0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#");
			printf("\n# Aguarde\n#\n");
			printf("##################################################################");
			sleep(3);
			justadd=0;
			activeContact = ContactList.first;
		}

		else if(activeContact == NULL){ //Se por algum motivo, o contato atual é nulo, uma pausa.
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
		if(activeContact!=NULL) setGlobalActive(activeContact->address); //Atualizar variável global.

		__fpurge(stdin);
		fgets(buffer,956,stdin);

		messagetype = parseMessage(buffer); //Tratar buffer e ver que tipo de mensagem o usuário entrou.

		if(removedTab){ //Se a aba atual foi deletada pelo outro usuário
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
			else activeContact = ContactList.first; //Passar à frente, estilo lista circular.
			break;

			case 5: //"Tab com nome":
			if(activeContact != NULL){
				connection* Marker = activeContact;
				activeContact = ContactList.first;
				while(activeContact != NULL){
					if(strcmp(activeContact->username,buffer) == 0) break;
					activeContact = activeContact->next; //Fazer uma busca pelo nome entrado.
				}

				if(activeContact == NULL){ //Se não encontrou
					activeContact = Marker; //Voltar para qual estava antes do comando.
					setvbuf(stdout, NULL, _IONBF,0);
					printf("\33[H\33[2J");
					printf("##################################################################\n#\n#");
					printf(" O contato especificado nao existe!\n#\n");
					printf("##################################################################");
					sleep(1);
				}

			}
			else{ //Se não tiver contatos em sua lista
				setvbuf(stdout, NULL, _IONBF,0);
				printf("\33[H\33[2J");
				printf("##################################################################\n#\n#");
				printf(" Nao ha contatos adicionados!\n#\n");
				printf("##################################################################");
				sleep(1);
			}
			break;

			case 3: //"Exit":
			setvbuf(stdout, NULL, _IONBF, 0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n");
			printf("# Terminando o programa!\n");
			printf("#\n##################################################################");			
			sleep(1);
			printf("\33[H\33[2J");
			quit = 1; //Mandar fechar todas as threads.
			break;

			case 0: //"Add":
			addAddress(buffer,0);
			break;

			case 2: //"Rem":
			if(activeContact != NULL && strcmp(activeContact->username,buffer) == 0) { //Se estiver removendo o contato atual
				if(activeContact->next != NULL) activeContact = activeContact->next; //Atual = Próximo se tiver próximo
				else if (strcmp(activeContact->address,ContactList.first->address) == 0) activeContact = NULL; //Atual = NULL se este era o último contato
				else activeContact = ContactList.first; //Atual = início de fila caso contrário.
				}
			removeContact(buffer);
			break;

			case -1: //"Msg":
			if(activeContact != NULL){
				if(activeContact->online == 1){
					sendMessage(activeContact->address,buffer,0); //Enviar mensagem só se estiver online.
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

			case 7: //"Fresh":
			setvbuf(stdout, NULL, _IONBF,0);
			printf("\33[H\33[2J");
			printf("##################################################################\n#\n#");
			printf(" Atualizando!\n#\n");
			printf("##################################################################");
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

			case 6: //"Group":
			groupMessage(buffer);
			break;

			case 1: //"Help":
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
	}
}

void sendMessage(char* address, char* message, int control){ //Função principal de mandar mensagem.
	pthread_mutex_lock(&sendMutex);
	int socket_id;
	struct hostent* host;
	struct sockaddr_in server_address;
	int connection_id;
	int i;
	int bytes_received;

	host = gethostbyname(address);

	if(host == NULL){ //Se a mensagem foi endereçada à algum endereço inválido

		logMsg("Erro no endereco");

		printf("\33[H\33[2J");
		printf("##################################################################\n#\n#");
		printf(" Erro no endereco\n");	
		printf("#\n##################################################################");
		sleep(3);
		pthread_mutex_unlock(&sendMutex);
		return;

	}
	
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
		printf("##################################################################\n#\n#");
		printf(" Erro: %s\n", strerror(errno));
		printf("#\n##################################################################");
		close(socket_id);
		sleep(3);
	}

//Criado o socket de envio
	else{
		if(!control){ //Se for uma mensagem de controle
			printf("\33[H\33[2J");
			if(message[0] == ':' && message[1] == 'a'){
				printf("##################################################################\n#\n#");
				printf(" Adicionando %s\n", address);	
				printf("#\n##################################################################");
				if(isEmpty()) justadd=1; //Primeiro adicionado da lista
					
			}
			else if(message[0] == ':' && message[1] == 'r'){
				printf("##################################################################\n#\n#");
				printf(" Removendo %s\n", address);	
				printf("#\n##################################################################");
					
			}
		}
		send(socket_id,message,strlen(message),0); //Enviar mensagem
		if(message[0]!=':') saveListMsg(0,address,message); //Salvar no log da conversa se foi uma mensagem de conversa
		close(socket_id); //Fechar conexão
		if(!control && (message[0] == ':' && message[1] == 'a' || message[0] == ':' && message[1] == 'r')) sleep(3); //Dar uma pausa

	}

	pthread_mutex_unlock(&sendMutex);
}

void addAddress(char* address, int control){
	char addMessage[1024];
	strcpy(addMessage,":a ");
	strcat(addMessage,thisUsername); //Enviar uma mensagem ":a <Username>" para ser adicionado no destino
	sendMessage(address,addMessage,control);
}

void addContact(char* address, char* username){
	pthread_mutex_lock(&pingMutex);

	connection* iterator = searchContact(address);

	if(iterator==NULL){ //Se não tiver o endereço nos contatos salvos
		iterator = ContactList.first;

		connection* newConnection = malloc(sizeof(connection)); //Criar um novo nó na lista
		strcpy(newConnection->address,address);
		strcpy(newConnection->username,username);
		newConnection->online=1;
		newConnection->counter=3;
		newConnection->next = NULL;

		if(iterator == NULL) ContactList.first = newConnection; //Se não tinha ninguém na lista, adicionar ao começo
		else { //Senão adicionar no final
			while(iterator->next != NULL) iterator = iterator->next;
			iterator->next = newConnection;
		}

		ContactList.size = ContactList.size + 1;
	}

	else //Se já existia essa conexão, atualizar dados
	{
		iterator->online=1;
		iterator->counter=3;
		strcpy(iterator->username,username);		
	}
	
	pthread_mutex_unlock(&pingMutex);
}

void addContactRemote(char* address, char* username){ //Mesma coisa que addContact, porém...
	pthread_mutex_lock(&pingMutex);
	connection* iterator = searchContact(address);
	if(iterator==NULL){ // usuario n existente

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
	else // usuario ja existente
	{
		iterator->online=1;
		iterator->counter=3;
		strcpy(iterator->username,username);		
	}
	pthread_mutex_unlock(&pingMutex);
	char addMessage[1024];
	strcpy(addMessage,":k "); //Enviar mensagem de confirmação ao remetente, para adicioná-lo de volta
	strcat(addMessage,thisUsername);
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

void removeContactRemote(char* address){ //Igual ao removeContact, exceto...
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

		if(strcmp(iterator->address,globalActive)==0) removedTab=1; //Se está removendo a aba atual, sinalizar

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

connection* searchContact(char* address){
	connection* iterator = ContactList.first;
	if(iterator == NULL) return; //Se não tem começo
	else if(strcmp(iterator->address,address) == 0) { //Se for o primeiro
		return iterator;
	}
	else { //Procurar o resto da lista encadeada
		while((iterator->next != NULL)) {
			if(strcmp(iterator->next->address,address) == 0)
				return iterator->next;
			iterator=iterator->next;
		}
	}

	return NULL; //Não achou
}

void setGlobalActive(char* address){
	globalActive = address;
}

int isEmpty(){ // apenas para controle, se a lista esta vazia ou nao
	connection* iterator = ContactList.first;
	if(iterator == NULL) return 1;
	else return 0;
}



void parseReceived(char* address, char* message){ //Manipulações com strings em C

	if(message[0] == ':'){ //Se começar com :, mensagem de controle

		char* Sep = strchr(message,' '); //Achar o primeiro espaço
		if(Sep == NULL) Sep = strrchr(message,'\0'); //Se falhou, achar o final da string

		char ParseCode[1024];
		strncpy(ParseCode,message,(Sep - message + 1)); //Copiar a parte de controle da mensagem
		ParseCode[(Sep - message + 1)] = '\0'; //Terminar a string ParseCode

		if(strstr(ParseCode,":a")){ //Adicionando Contato

			char* Separator = strchr(message,' '); //Pegar tudo depois do primeiro espaço
			logMsg("Added contact ");
			logMsg(address);
			addContactRemote(address,Separator+1);

		}

		else if(strstr(ParseCode,":k")){ //Recebendo uma confirmação de adição

			char* Separator = strchr(message,' ');
			logMsg("Added contact ");
			logMsg(address);
			addContact(address,Separator+1);
		}

		else if(strstr(ParseCode,":r")){ //Removendo Contato

			logMsg("Removed contact ");
			logMsg(address);
			removeContactRemote(address); //Remover quem enviou a mensagem

		}
		else if(strstr(ParseCode,":o")){ //Atualizar campos de Online

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

	else { //Se chegou mensagem de conversa, salvar no log de chat
		saveListMsg(1,address,message);
	}
}

int parseMessage(char* message){ //Dividir input de usuário
	int returnvalue;

	if(message[0] == ':'){ //Se for mensagem de comando

		char* Separator = strchr(message,' '); //Pegar o primeiro espaço
		if(Separator == NULL) Separator = strrchr(message,'\n'); //Se não tiver espaço, pegar o final do input
		char ParseCode[1024];
		strncpy(ParseCode,message,(Separator - message + 1));
		ParseCode[(Separator - message + 1)] = '\0'; //ParseCode = comando do usuário

		if(strstr(ParseCode,":a")){ //Comando de adicionar contato
			returnvalue = 0;
			//Código para copiar do separador ao final da string no buffer original para uso no messengerthread
			char* Separator2 = strrchr(message,'\n');
			Separator2[0] = '\0';
			if(Separator != Separator2) { //Se tiver algo depois do :a
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
			}
		}


		else if(strstr(ParseCode,":h")) returnvalue = 1; //Caso help

		else if(strstr(ParseCode,":r")){ //Comando de remover contato
			returnvalue = 2;

			char* Separator2 = strrchr(message,'\n');
			*Separator2 = '\0'; //Ver o final da string e terminar.

			if(Separator != Separator2) {
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
			}
		}

		else if(strstr(ParseCode,":q")) returnvalue = 3; //Quitar do programa

		else if(strstr(ParseCode,":t")){ //Caso de tab
			returnvalue = 4; //Tab sem username
			char* Separator2 = strrchr(message,' '); //Procurar o último espaço do input
			if(Separator2 != NULL) { //Se tiver espaço...
				Separator = strrchr(message,'\n');
				*Separator = '\0';

				char aux[1024];
				strcpy(aux,Separator2+1);
				strcpy(message,aux);

				returnvalue = 5; //Tab com username
			}
		}

		else if(strstr(ParseCode,":g")) { //Comando de mensagem em grupo
			returnvalue = 6;
			char* Separator2 = strrchr(message,'\n');

			if(Separator != Separator2) {
				char aux[1024];
				strcpy(aux,Separator+1);
				strcpy(message,aux);
			}
		}

		else if(strstr(ParseCode,":f")) returnvalue = 7; //Comando de refresh

		else if(strstr(ParseCode,":l")) returnvalue = 8; //Comando de listagem

		else{
			returnvalue=99; //Comando nenhum
		}
	}

	else{
		returnvalue = -1; //Mensagem de chat
	}

	return returnvalue;
}

void groupMessage(char* buffer){ //Função de mensagem em grupo

	char* identifier = buffer;
	int counter = 0;
	int i;

	while(identifier[0]!='\0'){ //Busca por quantos usernames tem
		if(identifier[0]=='@') counter++;
		identifier++;
	}

	char userSelector[64][64]; //Suporte a até 64 usernames com 64 chars cada

	if(!counter) return; //Se não teve nenhum username, sair

	char* identifier2 = buffer;
	identifier = buffer;
	i = 0;
	char setupFlag = 0;
	while(identifier[0]!='\0'){ //Enquanto não chegou ao final

		if(identifier[0]=='@'){ //Achou um username, espera-o terminar
			identifier++;
			identifier2 = identifier;
			setupFlag = 1;
		}

		else if(identifier[0]==' ' && setupFlag == 1){ //Terminou um username, espere outro começar
			identifier[0] = '\0';
			strcpy(userSelector[i],identifier2); //Copiar username encontrado para buffer
			identifier2 = identifier+1;
			while(identifier2[0]==' ') identifier2++;
			identifier = identifier2;
			i++; //Aumentar quantos usernames foram copiados
			setupFlag = 0;
		}

		else identifier++;
	}

	//if(setupFlag == 1) Não teve mensagem pra enviar, dur.
	//else identifier2 terá a mensagem para enviar.

	if(!setupFlag) {

		strcpy(buffer,identifier2); //Copiar mensagem de multicast para buffer
		pthread_mutex_lock(&pingMutex);
		connection* iterator = ContactList.first; //Iterar lista de contatos

		if(iterator != NULL)
			for(i = 0; i < counter; i++){
				while(iterator != NULL && strcmp(iterator->username,userSelector[i])!=0)
					iterator = iterator->next;
				if(iterator != NULL){ //Encontrou username na lista de contatos

					if(iterator->online == 1){ //Se online...
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

void printContactList(void){ // imprime lista de contatos
	setvbuf(stdout, NULL, _IONBF,0);
	printf("\33[H\33[2J");
	printf("##################################################################\n#\n#");
	printf(" Imprimindo lista de contatos:\n#");

	connection* iterator = ContactList.first;

	while(iterator != NULL){ // enquanto tem elementos na fila
		if(iterator->online) // se esta online
			printf("\n#\n# Username: %s\n# Address:  %s\n# Online:   Sim",iterator->username,iterator->address); 
		else		
			printf("\n#\n# Username: %s\n# Address:  %s\n# Online:   Nao",iterator->username,iterator->address); 
		iterator = iterator->next;
	}

	printf("\n#\n# Entre com qualquer linha para continuar.\n");
	getc(stdin);
}

void saveContacts(void){ //Ideia antiga, de salvar contatos em um .txt externo
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

void loadContacts(void){ //idem
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

void logMsg(char* Content){ //Salvar mensagem no log, para analise de erros e depuracao
	FILE* LogFile = fopen("log.txt","a");
	fputs(Content,LogFile);
	fputc('\n',LogFile);
	fclose(LogFile);
}

void saveListMsg(int incoming, char* address, char* message){ // salva o log de conversa
	//printf(" salvando ");
	conversation* iterator = MessageList.first;
	conversation* newConversation= malloc(sizeof(conversation));
	newConversation->incoming=incoming;
	newConversation->next=NULL;
	strcpy(newConversation->address,address);
	strcpy(newConversation->message,message);	
	if(iterator == NULL){ // lista vazia
		//printf(" eranulo ");
		MessageList.first = newConversation;
	}
	else{ // ha elementos na lista
		//printf(" neranulo ");
		while(iterator->next!=NULL)
			iterator=iterator->next;
		iterator->next = newConversation;
	}
	//printf(" terminei salvar "); sleep(3);
}

void printListMsg(char* address){ // imprime a conversa na tela
	conversation* iterator = MessageList.first;
	connection* user = searchContact(address);
	if(iterator == NULL || address == NULL){ // lista vazia ou nao ha contatos
		printf("\n#");
		return;
	}
	else{
		printf("\n#\n");
		if(strcmp(iterator->address,address)==0) // primeiro elemento da lista
		{
			//printf("1st %d ", iterator->incoming);
			if(!iterator->incoming) printf("#\n# %s: %s", thisUsername,iterator->message); // incoming indica se a mensagem foi enviada ou recebida
			else if(iterator->incoming) printf("#\n# %s: %s", user->username, iterator->message);
		}
		while(iterator->next!=NULL) // demais elementos
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

void printest(char* address){ //Teste de print
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
	char* ReturnOfNewlineKiller = strrchr(thisUsername,'\n'); //NEWLINEKILLER RETURNS
	*ReturnOfNewlineKiller = '\0'; //Esse carinha já salvou 3 trabalhos. Viva Newline Killer.
	quit = 0;
	justadd = 0;
	removedTab = 0;
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

