#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_INPUT 256
#define MAX_PATHS 128

void sigint_signalhandler(int);
int call_count=0;

int main(int argc, char *argv[]) {
    char *paths[MAX_PATHS];
    int num_paths = 0;
    char line[MAX_INPUT];
    char command_path[MAX_INPUT];
    int last_exit_status=0;


    int fd = 0;
    
    if ((fd = open(".myshell", O_RDONLY)) < 0)
    {
        perror("open error: .myshell");
        exit(-1);
    }

    char buf[MAX_INPUT];
    int size = 0;
    
    if ((size = read(fd, buf, sizeof(buf) - 1)) < 0)
    {
        perror("read error: .myshell");
        close(fd);
        exit(-2);
    }

    buf[size] = '\0';
    close(fd);

    if (!strncmp(buf, "PATH=", 5)) 
    {
        char *start = buf + 5;
        char *end = start;

        while (*end != '\0' && num_paths < MAX_PATHS) 
        {
            while (*end != ':' && *end != '\0') 
            {
                end++;
            }

            int length = end - start;
            paths[num_paths] = (char *)malloc(length + 1);
            if (paths[num_paths] == NULL) 
            {
                perror("malloc");
                exit(-3);
            }

            strncpy(paths[num_paths], start, length);
            paths[num_paths][length] = '\0';

            num_paths++;
            if (*end == ':') 
            {
                end++;
            }
            start = end;
        }
    }

    while (1) {
        // cygwin 환경이라 %%를 두 번 입력했습니다.
        // $로 바꿔도 가능합니다.

        printf("%% ");

        // ^C를해도 shell이 계속 실행되게 signal을 사용했습니다. 

        signal(SIGINT,sigint_signalhandler);

        if (!fgets(line, sizeof(line), stdin)) 
        {
            break;
        }

               
        
        line[strcspn(line,"\n")]=0;

        if (strlen(line) == 0) 
        {
            continue;
        }
        
        /* child exit을 보기위한 코드 
          옵션 찾을 수 없을 때 : 1  ex) date -x
          정상 수행: 0 또는 status  ex) date
        */

        if(strcmp(line,"echo ${?}")==0)
        {
            printf("%d\n",last_exit_status>>8);
            continue;
        }

        char *args[MAX_INPUT / 2 + 1];
        int arg_count = 0;
        char *command = strtok(line, " ");

        while (command != NULL && arg_count < MAX_INPUT / 2) 
        {
            args[arg_count++] = command;
            command = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (arg_count == 0)
        {
            continue;
        }

      

        int found = 0;
        for (int i = 0; i < num_paths; i++) 
        {
            // 사용자가 /bin/ls, /bin/date, /bin/echo와 같이 입력했을때 코드입니다. 
            // 각각의 옵션과 결합하여 수행 가능하도록 구현했습니다.

            if((!strncmp(args[0],"/bin",4))||(!strncmp(args[0],"/usr/bin",8))||
                (!strncmp(args[0],"/usr/local/bin",14))) 
            {
                strcpy(command_path, args[0]);   
                if (access(command_path, F_OK) == 0) 
                {
                    found = 1;
                    break;
                }
 
            }
            
            // ls, cat, cp, echo, rm, date, ps 같이 단일 명령으로 들어왔을 때 코드입니다.
            // 각각의 옵션과 결합하여 수행 가능하도록 구현했습니다.

            else if (strlen(paths[i]) + strlen(args[0]) + 1 < sizeof(command_path)) 
            {
                strcpy(command_path, paths[i]);
                strcat(command_path, "/");
                strcat(command_path, args[0]);
                if (access(command_path, F_OK) == 0) 
                {
                    found = 1;
                    break;
                }
            }
            else 
            {
                fprintf(stderr, "Command path long\n");
                last_exit_status=1;
            }
        }


        if (found) 
        {
            int pid = 0;

            if ((pid =fork()) < 0) 
            {
                perror("fork failed");
                exit(-4);

            } 
            else if (pid == 0) 
            {
                execv(command_path, args);
                perror("execv failed");
                exit(-5);
            }
            else
            {
                int status;
                wait(&status);
                last_exit_status=status; 
            }
        } 
        else 
        {
            printf("Command not found\n");
            last_exit_status=1;  
        }
        

    }
    

    for (int i = 0; i < num_paths; i++) 
    {
        free(paths[i]);
    }

    return 0;
}
// 종료 되지않고 ^C만 출력하도록 구현했습니다.
// ^C 후에는 다른 명령어를 입력해야 쉘(%)이 출력됩니다.(Signal 수행 후 리셋)

void sigint_signalhandler(int sig)
{
    call_count++;
    if(call_count ==1)
    {
        printf("^C\n");
    }

    else
    {
        printf("%% ^C\n");
    } 
}
