/**
 * CS6456 Homework 5: A simple FTP server
 * Author: Lu Tian
 *
 * This file implements a bare-bone FTP server. It only supports the
 * commands listed in Section 5.1 in RFC959, as well as the LIST command
 *
 * This implementation only supports one client at a time.
 *
 * Three functions are implemented
 * main(): the main function
 * tokenize(char *str, char **args, char *delim): split the incoming command into tokens separated by @delim
 * data_channel_connect(): establish the data connection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define MAX_COMMAND_LENGTH 100

#define TYPE_IMAGE 1
#define TYPE_ASCII 0

///All global variables
///states of the ftp server
int port_number;
struct sockaddr_in listen_sock_addr;
int listen_sock_fd;
int connection_fd;
struct sockaddr_in peer_addr;
int data_trans_fd;
struct sockaddr_in data_trans_addr;
char * pwd = NULL;
short int type_state = TYPE_ASCII; /// how is the bytes transferred, initially ASCII
/// actually we only support IMAGE. So we are forcing the client to change it explicitly.

char message_504[] = "504 Command not implemented for that parameter.\r\n";
int message_length_504 = 49;
char message_200[] = "200 Command okay.\r\n";
int message_length_200 = 19;
char message_220[] = "220 my FTP server\r\n";
int message_length_220 = 19;
char message_221[] = "221 Good bye.\r\n";
int message_length_221 = 15;
char message_230[] = "230 User logged in, proceed.\r\n";
int message_length_230 = 30;
char message_502[] = "502 Command not implemented.\r\n";
int message_length_502 = 30;
char message_501[] = "501 Syntax error in parameters or arguments.\r\n";
int message_length_501 = 46;
char message_150[] = "150 File status okay; about to open data connection.\r\n";
int message_length_150 = 54;
char message_450[] = "450 Requested file action not taken.\r\n";
int message_length_450 = 38;

char message_226[] = "226 Closing data connection.\r\n";
int message_length_226 = 30;

char message_425[] = "425 Can't open data connection.\r\n";
int message_length_425 = 33;

char message_250[] = "250 Requested file action okay, completed.\r\n";
int message_length_250 = 44;

char message_451[] = "451 Requested action aborted: local error in processing.\r\n";

/// Split a line into arguments separated by deliminators
/// @str input line
/// @args an array of pointers to store the arguments. The user is responsible to allocate
/// memory for the first level array.
/// @delim deliminator
/// @return the number of arguments in the string.
/// At the end, @args will contain pointers to
/// strings located inside @str. The end of the list is NULL
/// This function does not allocate memory in heap
int tokenize(char *str, char **args, const char *delim);

int data_channel_connect();

int main(int argc, char **argv)
{
    if(argc < 2)
    {

        printf("Usage: my_ftpd port_number\n");
        return 0;
    }

    port_number = atoi(argv[1]);
    if(port_number <= 1024)
    {
        printf("Please specify a port number greater than 1024");
        return 0;
    }
    listen_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_fd == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    else printf("creating socket succeed\n");
    memset(&listen_sock_addr, 0, sizeof(struct sockaddr_in));
    listen_sock_addr.sin_family = AF_INET;
    listen_sock_addr.sin_port = htons(port_number);
    listen_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listen_sock_fd,
            (struct sockaddr *)&listen_sock_addr, sizeof(struct sockaddr_in)) == -1 )
    {
        perror("cannot bind");
        close(listen_sock_fd);
        exit(EXIT_FAILURE);

    }
    else printf("bind succeed\n");

    if(listen(listen_sock_fd, 10) == -1)
    {
        perror("listen failed");
        close(listen_sock_fd);
        exit(EXIT_FAILURE);
    }
    else printf("listen succeed\n");
    char buf[MAX_COMMAND_LENGTH+1];
    char * args [MAX_COMMAND_LENGTH / 2];
    unsigned int peer_addr_len = sizeof(struct sockaddr_in);
    while(true)
    {

        connection_fd = accept(listen_sock_fd, (struct sockaddr *)&peer_addr, (socklen_t *)&peer_addr_len);
        if (connection_fd < 0)
        {
            perror("accept failed");
            close(listen_sock_fd);
            exit(EXIT_FAILURE);
        }
        else printf("accept succeeded\n");

        /// test the client IP
        uint32_t client_ip_32bit = ntohl(peer_addr.sin_addr.s_addr);
        printf("Client IP: %d.%d.%d.%d\n", client_ip_32bit >> 24,
               (client_ip_32bit>>16) & 0xFF,
               (client_ip_32bit>>8) & 0xFF,
               (client_ip_32bit) & 0xFF );
        /// End test

        /// set the peer IP and default ports
        memset( &data_trans_addr, 0, sizeof(sockaddr_in));
        data_trans_addr.sin_port = peer_addr.sin_port;
        data_trans_addr.sin_family = AF_INET;
        data_trans_addr.sin_addr.s_addr = peer_addr.sin_addr.s_addr;

        /// print the greeting message
        write(connection_fd, message_220, message_length_220 );
        bool quit = false;
        do
        {
            int command_length;
            command_length = read(connection_fd, buf, MAX_COMMAND_LENGTH);
            buf[command_length]=0;
            if(command_length == 0) { /// if the connection lost
                quit = true;
                break;
            }
            int num_arg = tokenize(buf, args, " \r\n");
            int i;
            printf("%d ", num_arg);
            for (i = 0; i < num_arg ; i++)
            {
                printf("%s ", args[i]);
            }
            putchar('\n');
            if(strcmp(args[0],"QUIT") == 0)  /// client wants to quit
            {
                quit = true;
                write(connection_fd, message_221, message_length_221 );
            }
            else if(strcmp(args[0],"USER") == 0) // client want to log in
                write(connection_fd, message_230, message_length_230 );
            else if (strcmp(args[0], "STRU") == 0)   // The structure of data.
            {
                if(strcmp(args[1], "F")) /// only support FILe structure
                    write(connection_fd, message_200, message_length_200);
                else
                    write(connection_fd, message_504, message_length_504 );
            }
            else if (strcmp(args[0], "NOOP") == 0)
            {
                write(connection_fd, message_200, message_length_200);
                continue;
            }
            else if(strcmp(args[0], "TYPE") == 0)
            {
                if(num_arg < 2) type_state = TYPE_IMAGE; ///default is image
                else if(strcmp(args[1], "I") == 0)
                    /// if specified I, set correct type
                    type_state = TYPE_IMAGE;
                else /// else, set type as 2, means not supported
                    type_state = TYPE_ASCII;
                if(type_state == TYPE_IMAGE)
                    write(connection_fd, message_200, message_length_200);
                else
                    write(connection_fd, message_504, message_length_504);
            }
            else if(strcmp(args[0], "MODE") == 0)     /// only need to implement stream mode
            {
                if(num_arg < 2 || (strcmp(args[1],"S") == 0) )
                {
                    write(connection_fd, message_200, message_length_200);
                }
                else
                    write(connection_fd, message_504, message_length_504);
            }
            else if(strcmp(args[0], "PORT") == 0)
            {
                if(num_arg < 2)
                {
                    write(connection_fd, message_501, message_length_501);
                    continue;
                }
                int addr1, addr2, addr3,addr4,addr5,addr6;
                sscanf(args[1], "%d,%d,%d,%d,%d,%d", &addr1, &addr2, &addr3, &addr4, &addr5, &addr6);
                data_trans_addr.sin_port = htons(addr5 * 256 + addr6);
                write(connection_fd, message_200, message_length_200);
            }
            else if(strcmp(args[0], "RETR") == 0)
            {
                if(num_arg < 2)   /// filename not provided
                {
                    write(connection_fd, message_501, message_length_501);
                    continue;
                }

                if(type_state != TYPE_IMAGE) {
                    write(connection_fd, message_451, sizeof(message_451) - 1);
                    continue;
                }
                
                
                /// filename is provided
                
                int retr_file_fd = open(args[1],O_RDONLY);
                if(retr_file_fd == -1)
                {
                    /// file cannot be opened
                    printf("cannot find file: %s\n", args[1][0] == '/' ? args[1] + 1 : args[1]);
                    write(connection_fd, message_450, message_length_450);
                    continue;
                } else {
                    /// tell the client that the file has been found
                    write(connection_fd, message_150, message_length_150);
                }
                
                if(data_channel_connect()!= 0) {
                    write(connection_fd, message_425, message_length_425);
                    close(retr_file_fd);
                    continue;
                }
                char *f_buf = (char *)malloc(65536); /// the buffer storing file contents
                int tmp = 0; /// stores the actual bytes read from local file
                while(tmp = read(retr_file_fd, f_buf, 65536), tmp > 0)
                {
                    /// send the content of the file
                    write(data_trans_fd, f_buf, tmp);
                    printf("Read %d bytes from file.\n", tmp);
                }
                puts("About to close local file");
                close(retr_file_fd);
                write(connection_fd, message_226, message_length_226);
                puts("About to close data connection.");
                close(data_trans_fd);
                //write(connection_fd, message_250, message_length_250);
                free(f_buf); f_buf = NULL;
            } else if(strcmp(args[0], "STOR") == 0) {
                if(num_arg < 2)   /// filename not provided
                {
                    write(connection_fd, message_501, message_length_501);
                    continue;
                }

                if(type_state != TYPE_IMAGE) {
                    write(connection_fd, message_451, sizeof(message_451) - 1);
                    continue;
                }
                
                
                int stor_file_fd = creat(args[1],0644);
                if(stor_file_fd == -1) {
                    /// file cannot be opened
                    printf("cannot create file: %s\n", args[1][0] == '/' ? args[1] + 1 : args[1] );
                    write(connection_fd, message_450, message_length_450);
                    continue;
                } else {
                    /// tell the client that this filename is available
                    write(connection_fd, message_150, message_length_150);
                }
                
                if(data_channel_connect() != 0) {
                    write(connection_fd, message_425, message_length_425);
                    close(stor_file_fd);
                    continue;
                }
                
                char *f_buf = (char *)malloc(65536); /// the buffer storing file contents
                int tmp = 0; /// stores the actual bytes read from local file
                while(tmp = read(data_trans_fd, f_buf, 65536), tmp != 0)
                {
                    /// send the content of the file
                    write(stor_file_fd, f_buf, tmp);
                    printf("Write %d bytes to file.\n", tmp);
                }
                puts("About to close local file");
                close(stor_file_fd);
                write(connection_fd, message_226, message_length_226);
                puts("About to close data connection.");
                close(data_trans_fd);
                //write(connection_fd, message_250, message_length_250);
                free(f_buf); f_buf = NULL;
            }
            else if(strcmp(args[0], "LIST") == 0)
            {
                char cmdln_arg0[] = "ls";
                char cmdln_arg1[] = "-l";
                
                /// command line framework for ls
                char * ls_arglist[4] = {cmdln_arg0, cmdln_arg1, NULL, NULL};
                
                
                /// If the path does not start with '/', then pass it to ls directly
                /// If it starts with '/' and there is something more, then ignore the
                /// starting '/'. If the path is only a '/', then do not pass anything to ls
                if(num_arg >= 2) ls_arglist[2] = args[1][0] != '/' ? args[1] : 
                    (args[1][1] == '\0' ? NULL : args[1] + 1);
                int tmp_pipe[2];
                pipe(tmp_pipe);
                pid_t a;
                a = fork();
                if(a == 0){
                    dup2(tmp_pipe[1], STDOUT_FILENO);
                    close(tmp_pipe[0]);
                    if(execvp(ls_arglist[0], ls_arglist) ) {
                        exit(EXIT_FAILURE);
                    }
                } else if (a > 0){
                    close(tmp_pipe[1]);
                    
                    /// tell the client that the data are ready
                    write(connection_fd, message_150, message_length_150);
                    
                    if(data_channel_connect() != 0) {
                        write(connection_fd, message_425, message_length_425);
                        close(tmp_pipe[0]); continue;
                    }
                    char* f_buf = (char*)malloc(65536);
                    int tmp = 0; /// stores the actual bytes read from local file
                    while(tmp = read(tmp_pipe[0], f_buf, 65536), tmp != 0) {
                        /// send the content of the file
                        write(data_trans_fd, f_buf, tmp);
                        printf("read %d bytes from ls.\n", tmp);
                    }
                    close(tmp_pipe[0]);
                    write(connection_fd, message_226, message_length_226);
                    close(data_trans_fd);
                    free(f_buf);
                    f_buf = NULL;
                } else {
                    close(tmp_pipe[0]);
                    close(tmp_pipe[1]);
                    write(connection_fd, message_450, message_length_450);
                }
            }
            else
                write(connection_fd, message_502, message_length_502);
        }
        while (!quit);
        if(shutdown(connection_fd, SHUT_RDWR) == -1)
        {
            perror("shutdown failed");
            close(connection_fd);
            close(listen_sock_fd);
            exit(EXIT_FAILURE);
        }
        else printf("shutdown succeeded\n");
        close(connection_fd);
    }
    close(listen_sock_fd);

    return 0;
}

int tokenize(char * str, char ** args, const char * delim)
{
    char * p = strtok(str, delim);
    int i = 0;
    while(p != NULL)
    {
        args[i] = p;
        p = strtok(NULL, delim);
        i++;
    }
    args[i] = NULL;

    return i;
}


/// open the data connection
/// @return 1 - something wrong in connection
/// @return 0 - connect succeesfully
int data_channel_connect()
{
    if((data_trans_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        puts("Cannot create data connection socket.");
        return 1;
    }
    if(connect(data_trans_fd, (struct sockaddr*)&data_trans_addr, sizeof(struct sockaddr_in)) == -1) {
        puts("data connection failed.");
        close(data_trans_fd);
        return 1;
    }
    return 0;
}
