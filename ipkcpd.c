//       BRNO UNIVERSITY OF TECHNOLOGY
// COMPUTER COMMUNICATIONS AND NETWORKS COURSE
//               PROJECT N.2
//--------------------------------------------
//          AUTHOR: MICHAL BELOVEC
//           LOGIN: XBELOV04


#ifdef __unix__

    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>

#elif defined(_WIN32) || defined(WIN32)

    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>

#endif

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <poll.h>

#define MAX_CLI 100

void sigHandler(int);
char* compute(char* res);
int getnum(char c);

//Variables
int mode = -1; //TCP -> 0, UDP -> 1
int opt = 1;
int serv_sock;
int client_sock;
char buf[258];
struct sockaddr_in sa_client;
struct sockaddr_in sa;
socklen_t sa_client_len = sizeof(sa_client);
struct hostent *server;
int err = 0;
int port_number;

int main(int argc, char* argv[]){
    int h_arg_index = -1;
    int p_arg_index = -1;
    if(argc == 1){
        fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
        exit(EXIT_FAILURE);
    }
    if(argc < 7 && strcmp(argv[1], "--help") != 0){
        fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
        exit(EXIT_FAILURE);
    }
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i],"-h") == 0){//Found -h the next arg should be the host name or IP adress
            i++;
            if(i < argc){
                h_arg_index = i;
            }
            else{//Ran out of arguments
                fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
                exit(EXIT_FAILURE);
            }
        }
        else if(strcmp(argv[i],"-p") == 0){//Found -p the next arg should be the port number
            i++;
            if(i < argc){
                p_arg_index = i;
            }
            else{//Ran out of arguments
                fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
                exit(EXIT_FAILURE);
            }
        }
        else if(strcmp(argv[i],"-m") == 0){
            i++;
            if(i < argc){
                if(strcmp(argv[i], "tcp") == 0){
                    mode = 0;
                }
                else if(strcmp(argv[i], "udp") == 0){
                    mode = 1;
                }
                else{
                    fprintf(stderr, "ERROR: unsupported mode!\nSupported modes: \"tcp\"|\"udp\".\n");
                    exit(EXIT_FAILURE);
                }
            }
            else{//Ran out of arguments
                fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
                exit(EXIT_FAILURE);
            }
        }
        else if(strcmp(argv[1], "--help") == 0){
            fprintf(stderr, "Correct usage is as follows: ./ipkcpd -h <host> -p <port> -m <mode>\nArguments might be in any order.\n");
            return 0;
        }
        else{//Found an argument that doesnt match any allowed arguments
            fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
            exit(EXIT_FAILURE);
        }
    }

    if(h_arg_index == -1 || p_arg_index == -1 || mode == -1){ //If there are missing arguments
        fprintf(stderr, "ERROR: incorrect arguments! Try \"--help\"!\n");
        exit(EXIT_FAILURE);
    }

    port_number = atoi(argv[p_arg_index]);
    
    /* 2. ziskani adresy serveru pomoci DNS */
    
    if ((server = gethostbyname(argv[h_arg_index])) == NULL) {
        fprintf(stderr,"ERROR: no such host as %s\n", argv[h_arg_index]);
        exit(EXIT_FAILURE);
    }
    
    /* 3. nalezeni IP adresy serveru a inicializace struktury server_address */
    bzero((char *) &sa, sizeof(sa));
    sa.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&sa.sin_addr.s_addr, server->h_length);
    sa.sin_port = htons(port_number);
    

    //The result will be in the placed in 'res'
    //getaddrinfo(argv[h_arg_index], argv[p_arg_index], &hints, &res);

    signal(SIGINT, sigHandler); //On detection of SIGINT, calls the sigHandler function
                                //which ends the connection gracefully.

    if(mode == 0){

        //Creating the server socket
        serv_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(serv_sock == -1){
            fprintf(stderr,"ERROR: error while creating server socket\n");
            exit(EXIT_FAILURE);
        }

        //Setting reuseaddr
        if(setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0){  
            fprintf(stderr, "ERROR: setsockopt\n");  
            exit(EXIT_FAILURE);  
        }  

        int bnd = bind(serv_sock, (const struct sockaddr *) &sa, sizeof(sa));
        //Binding the socket to the server address
        if(bnd < 0){
            fprintf(stderr,"ERROR: error while binding\n");
            exit(EXIT_FAILURE);
        }

        //Waiting for the client side connection
        if(listen(serv_sock, 32) < 0){
            fprintf(stderr,"ERROR: error while listening\n");
            exit(EXIT_FAILURE);
        }

        struct pollfd pollfds[MAX_CLI + 1];
        pollfds[0].fd = serv_sock;
        pollfds[0].events = POLLRDNORM;
        int clients = 0;

        for(int z = 1; z <= MAX_CLI; z++){
            pollfds[z].fd = -1;
        }

        int readlen = 0;
        int writelen = 0;
        bool hello[MAX_CLI];
        bool solve = false;
        char check[10];
        char res[256];
        strcpy(check, "SOLVE ");
        int pollres = 0;

        //Accepting a new client connection
        while(1){
            //Timeout set to 100 seconds
            pollres = poll(pollfds, clients + 1, NULL);
            if(pollres <= 0){
                continue; //Nothing new
            }
            //New client
            if(pollfds[0].revents & POLLRDNORM){
                client_sock = accept(serv_sock, (struct sockaddr *)&sa_client, &sa_client_len);
                if(client_sock < 0){
                    fprintf(stderr, "Error: accept\n");
                    exit(EXIT_FAILURE);
                }
                for (int j = 1; j < MAX_CLI; j++){
                    //Assigning the new client to the first empty space
                    if (pollfds[j].fd < 0)
                    {
                        pollfds[j].fd = client_sock;
                        pollfds[j].events = POLLRDNORM;
                        //Awaiting first message setting - on
                        hello[j] = true;
                        if(j > clients){
                            clients = j;
                        }
                        break;
                    }

                }
            }
            for (int k = 1; k <= clients; k++){
                if(pollfds[k].fd < 0){
                    continue;
                }
                //Incoming message
                if (pollfds[k].revents & (POLLRDNORM | POLLERR)){
                    //Reading the incomming message
                    bzero(buf, sizeof(buf));
                    if((read(pollfds[k].fd, buf, sizeof(buf) - 1)) <= 0){
                        pollfds[k].fd = -1;
                        pollfds[k].events = 0;
                        pollfds[k].revents = 0;
                        continue;
                    }
                    else{
                        //First message
                        if(hello[k]){
                            if(strcmp(buf, "HELLO\n") == 0){
                                //The first message was HELLO, continue communicating
                                hello[k] = false;

                                bzero(buf, sizeof(buf));
                                //Returning the HELLO message
                                strcpy(buf, "HELLO\n");
                                writelen = write(pollfds[k].fd, buf, strlen(buf));
                                if(writelen < 0){
                                    fprintf(stderr,"ERROR: error while sending message\n");
                                    close(serv_sock);
                                    exit(EXIT_FAILURE);
                                }
                            }
                            else{
                                //The first message was not HELLO
                                bzero(buf, sizeof(buf));
                                strcpy(buf, "BYE\n");
                                writelen = write(pollfds[k].fd, buf, strlen(buf));
                                if(writelen < 0){
                                    fprintf(stderr,"ERROR: error while sending message\n");
                                    close(serv_sock);
                                    exit(EXIT_FAILURE);
                                }
                                pollfds[k].fd = -1;
                                pollfds[k].events = 0;
                                pollfds[k].revents = 0;
                                break;
                            }
                        }
                        //Not the first message
                        else{
                            solve = true;
                            if(strlen(buf) >= 5){
                                for(int l = 0; l <= 5; l++){
                                    //Checking if the message begins with 'SOLVE '
                                    if(buf[l] != check[l]){
                                        solve = false;
                                        l = 6;
                                    }
                                }

                                if(solve){
                                    compute(res);
                                    //If the message is not formed well
                                    if(err == 1){
                                        bzero(buf, sizeof(buf));
                                        strcpy(buf, "BYE\n");
                                        writelen = write(pollfds[k].fd, buf, strlen(buf));
                                        if(writelen < 0){
                                            fprintf(stderr,"ERROR: error while sending message\n");
                                            close(serv_sock);
                                            exit(EXIT_FAILURE);
                                        }
                                        pollfds[k].fd = -1;
                                        pollfds[k].events = 0;
                                        pollfds[k].revents = 0;
                                        err = 0;
                                    }
                                    else{
                                        //Clearing the buffer
                                        bzero(buf, sizeof(buf));

                                        //Filling the buffer with the response
                                        strcpy(buf, "RESULT ");
                                        strcat(buf, res);
                                        strcat(buf, "\n");

                                        //Sending the response
                                        writelen = write(pollfds[k].fd, buf, strlen(buf));
                                        if(writelen < 0){
                                            fprintf(stderr,"ERROR: error while sending message\n");
                                            close(serv_sock);
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                }
                                else{
                                    //The message was not a valid expression,
                                    bzero(buf, sizeof(buf));
                                    strcpy(buf, "BYE\n");
                                    writelen = write(pollfds[k].fd, buf, strlen(buf));
                                    if(writelen < 0){
                                        fprintf(stderr,"ERROR: error while sending message\n");
                                        close(serv_sock);
                                        exit(EXIT_FAILURE);
                                    }
                                    pollfds[k].fd = -1;
                                    pollfds[k].events = 0;
                                    pollfds[k].revents = 0;
                                }
                            }
                            else{
                                //The message was not a valid expression,
                                //the message could be BYE or HELLO, but
                                //the behavior of the program is the same
                                bzero(buf, sizeof(buf));
                                strcpy(buf, "BYE\n");
                                writelen = write(pollfds[k].fd, buf, strlen(buf));
                                if(writelen < 0){
                                    fprintf(stderr,"ERROR: error while sending message\n");
                                    close(serv_sock);
                                    exit(EXIT_FAILURE);
                                }
                                pollfds[k].fd = -1;
                                pollfds[k].events = 0;
                                pollfds[k].revents = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    else if(mode == 1){
        //Creating the server socket
        serv_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(serv_sock == -1){
            fprintf(stderr,"ERROR: error while creating server socket\n");
            exit(EXIT_FAILURE);
        }

        //Setting reuseaddr
        if(setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0){  
            fprintf(stderr, "ERROR : setsockopt\n");  
            exit(EXIT_FAILURE);  
        }  

        int bnd = bind(serv_sock, (const struct sockaddr *) &sa, sizeof(sa));
        //Binding the socket to the server address
        if(bnd < 0){
            fprintf(stderr,"ERROR: error while binding\n");
            exit(EXIT_FAILURE);
        }

        char res[256];
        char supportArray[256];
        int payLen;

        while(1){
            //Clearing buffer before receiving
            bzero(buf, sizeof(buf));
            if((recvfrom(serv_sock, buf, sizeof(buf), 0, (struct sockaddr *)&sa_client, &sa_client_len)) == -1){
                fprintf(stderr,"ERROR: error recvfrom\n");
                close(serv_sock);
                exit(EXIT_FAILURE);
            }
            if(buf[0] == '\0'){
                compute(res);
                if(err == 1){
                    //Setting up the error message in a second buffer
                    bzero(supportArray, sizeof(supportArray));
                    strcpy(supportArray, "Not a well-formed message");
                    payLen = strlen(supportArray);

                    //Clearing the message buffer before writing into it
                    bzero(buf, sizeof(buf));
                    buf[0] = 'X';
                    buf[1] = 'X'; //Temporary values
                    buf[2] = 'X';
                    strcat(buf, supportArray);
                    buf[0] = '\1'; //Response opcode
                    buf[1] = '\1'; //Error status code
                    buf[2] = (char) payLen; //Message length

                    //Send the buffer with our message
                    if((sendto(serv_sock, buf, payLen+3, 0, (struct sockaddr *)&sa_client, sa_client_len)) == -1){
                        fprintf(stderr,"ERROR: error sendto\n");
                        close(serv_sock);
                        exit(EXIT_FAILURE);
                    }
                }
                else{
                    //Setting up the error message in a second buffer
                    payLen = strlen(res);

                    //Clearing the message buffer before writing into it
                    bzero(buf, sizeof(buf));
                    buf[0] = 'X';
                    buf[1] = 'X'; //Temporary values
                    buf[2] = 'X';
                    strcat(buf, res);
                    buf[0] = '\1'; //Response opcode
                    buf[1] = '\0'; //OK status code
                    buf[2] = (char) payLen; //Message length

                    //Send the buffer with our message
                    if((sendto(serv_sock, buf, payLen+3, 0, (struct sockaddr *)&sa_client, sa_client_len)) == -1){
                        fprintf(stderr,"ERROR: error sendto\n");
                        close(serv_sock);
                        exit(EXIT_FAILURE);
                    }
                }
            }
            else{
                //Setting up the error message in a second buffer
                bzero(supportArray, sizeof(supportArray));
                strcpy(supportArray, "The server only accepts requests");
                payLen = strlen(supportArray);

                //Clearing the message buffer before writing into it
                bzero(buf, sizeof(buf));
                buf[0] = 'X';
                buf[1] = 'X'; //Temporary values
                buf[2] = 'X';
                strcat(buf, supportArray);
                buf[0] = '\1'; //Response opcode
                buf[1] = '\1'; //Error status code
                buf[2] = (char) payLen; //Message length

                //Send the buffer with our message
                if((sendto(serv_sock, buf, payLen+3, 0, (struct sockaddr *)&sa_client, sa_client_len)) == -1){
                    fprintf(stderr,"ERROR: error sendto\n");
                    close(serv_sock);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

//Funtion that handles the SIGINT signal by terminating the connection
//and closing the socket
void sigHandler(int dummy){
    close(serv_sock);
    exit(EXIT_SUCCESS);
}

char* compute(char* res){
    err = 0;
    char forUdp[258];
    int x = 0;
    int Stack[256];
    int top = 0;
    int rBracketCount = 0;
    int lBracketCount = 0;
    int coef = 1;
    int num = 0;
    int numOfOperands = 0;
    int indexConstant;
    int payLength;
    if(mode == 0){
        //TCP - ignores the first 6 chars - which are 'SOLVE '
        indexConstant = 6;
    }
    else if(mode == 1){
        //UDP - reads the whole array
        indexConstant = 0;
        payLength = (int) buf[1];
        bzero(forUdp, sizeof(forUdp));
        for(int d = 2; d < payLength+2; d++){
            forUdp[d-2] = buf[d];
        }
        //The following cleans up the opcode and payload length
        //from the buffer, so it is ready to calculate
        bzero(buf, sizeof(buf));
        strcpy(buf, forUdp);
    }
    //The last 2 characters of the message must be '\n' and ')'
    if(buf[strlen(buf)-1] != '\n' || buf[strlen(buf)-2] != ')'){
        err = 1;
        return buf;
    }
    //Loop for checking if there is the same number of opening and closing brackets
    for(int j = strlen(buf)-2; j >= indexConstant ; j--){
        if(buf[j] == ')'){
            rBracketCount++;
        }
        else if(buf[j] == '('){
            lBracketCount++;
        }
    }
    if(rBracketCount != lBracketCount || rBracketCount == 0 || lBracketCount == 0){
        //Wrong bracketing
        err = 1;
        return buf;
    }
    //Reading the query from end to start
    for(int i = strlen(buf)-2; i >= indexConstant ; i--){
        if(buf[i] == '('){
            if(i-1 >= 0){
                //There must always be a space before a '('
                if(buf[i-1] != ' '){
                    err = 1;
                    return buf;
                }
            }
        }
        //Processing numbers and pushing them onto the Stack
        else if(buf[i] >= '0' && buf[i] <= '9'){
            while(buf[i] >= '0' && buf[i] <= '9'){
                //getnum - converts the char number into a int number
                num = getnum(buf[i]);
                x = x + (coef * num);
                coef = coef * 10;
                i--;
            }
            numOfOperands++;
            coef = 1;

            //Checking for space on the Stack
            if(top < 255){
                Stack[top] = x;
                x = 0;
                top++;
            }

            //There must always be a space before an operand
            if(buf[i] != ' '){
                err = 1;
                return buf;
            }
        }
        //Performing operations with the stack
        else if(buf[i] == '+'){
            //Performs 'n-1' operations if there are 'n' operands
            for(int o = 0; o < numOfOperands-1; o++){
                //Checking if there are enough operands on the stack
                if(top >= 2){
                    Stack[top-2] = Stack[top-1] + Stack[top-2];
                    top--;
                }
                else{
                    err = 1;
                    return buf;
                }
            }
            numOfOperands = 1;

            //Based on the IPKCP protocol, there must always be
            //a '(' char after the operator
            if(i-1 >= 0 && buf[i-1] != '('){
                err = 1;
                return buf;
            }
        }
        else if(buf[i] == '-'){
            //Performs 'n-1' operations if there are 'n' operands
            for(int o = 0; o < numOfOperands-1; o++){
                //Checking if there are enough operands on the stack
                if(top >= 2){
                    Stack[top-2] = Stack[top-1] - Stack[top-2];
                    top--;
                }
                else{
                    err = 1;
                    return buf;
                }
            }
            numOfOperands = 1;

            //Based on the IPKCP protocol, there must always be
            //a '(' char after the operator
            if(i-1 >= 0 && buf[i-1] != '('){
                err = 1;
                return buf;
            }
        }
        else if(buf[i] == '*'){
            //Performs 'n-1' operations if there are 'n' operands
            for(int o = 0; o < numOfOperands-1; o++){
                //Checking if there are enough operands on the stack
                if(top >= 2){
                    Stack[top-2] = Stack[top-1] * Stack[top-2];
                    top--;
                }
                else{
                    err = 1;
                    return buf;
                }
            }
            numOfOperands = 1;

            //Based on the IPKCP protocol, there must always be
            //a '(' char after the operator
            if(i-1 >= 0 && buf[i-1] != '('){
                err = 1;
                return buf;
            }
        }
        else if(buf[i] == '/'){
            //Performs 'n-1' operations if there are 'n' operands
            for(int o = 0; o < numOfOperands-1; o++){
                //Checking if there are enough operands on the stack
                if(top >= 2){
                    //Checking for division by 0
                    if(Stack[top-2] == 0){
                        err = 1;
                        return buf;
                    }
                    Stack[top-2] = Stack[top-1] / Stack[top-2];
                    top--;
                }
                else{
                    err = 1;
                    return buf;
                }
            }
            numOfOperands = 1;

            //Based on the IPKCP protocol, there must always be
            //a '(' char after the operator
            if(i-1 >= 0 && buf[i-1] != '('){
                err = 1;
                return buf;
            }
        }
        else if(buf[i] == ')'){
            //After the ')' char a number must follow
            if((buf[i-1] < '0' || buf[i-1] > '9') && buf[i-1] != ')'){
                err = 1;
                return buf;
            }
        }
        else if(buf[i] == ' '){
            continue;
        }
        else{
            err = 1;
            return buf;
        }
    }
    //Error when there is more than one item on the stack
    //(leftover numbers), or the result is negative - IPKCP prohibited
    //The program uses the same calculator modul for both TCP and UDP.
    if(top != 1 || Stack[top-1] < 0){
        err = 1;
        return buf;
    }
    //Converts the result into a string and prints it onto the 'res' buffer
    //which is then returned
    bzero(res, 256);
    sprintf(res, "%i", Stack[top-1]);
    return res;
}

//Funtion for converting char number to int number
int getnum(char c){
    switch(c){
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        default:
            break;
    }
}