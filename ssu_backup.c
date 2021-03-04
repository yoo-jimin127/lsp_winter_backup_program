#include "backupProgram.h"

#define MAX_SIZE 99999 // to store file content
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //쓰레드 초기화
int ncount; //쓰레드간 공유되는 자원
int result; // pthread_join -> return value

pthread_t p_thread[MAX_SIZE]; //스레드 식별자

//==========================구조체 선언부================================

//백업할 파일의 정보를 저장할 구조체
/*
   typedef struct fileInfo {
   char absPath[256]; // 파일의 절대경로를 저장하기 위한 문자열 배열
   int filePeriod; // 해당 파일의 백업 주기
//char fileOption[30]; //파일의 백업 옵션
}FileInfo; */

/*
//파일명으로 연결된 리스트의 노드
typedef struct node {
char absPath[256];
char fileName[256]; // 명세 : 백업할 파일명 길이 제한 255bytes
int filePeriod; // 해당 파일의 백업 주기

struct node *next;
struct node *prev;
}Node;

typedef struct backupNode {
pid_t pid;
pthread_t tid;
char fileName[256];
int period;
int backup_time;
int file_size;
char file_content[MAX_SIZE];

struct backupNode *next;
struct backupNode *prev;
}BackupNode;

//백업할 파일들을 연결해놓은 이중 연결리스트 (백업리스트)
typedef struct linkedList {
Node *head; //연결리스트의 head 노드
Node *tail; //연결리스트의 tail 노드
int fileCnt; //백업할 파일들의 개수를 세기위한 카운트
}LinkedList;


typedef struct backupContentList {
BackupNode *head;
BackupNode *tail;
int backup_perform_cnt;
}BackupContentList;
 */

//================================함수 선언부=======================================

int main (int argc, char* argv[]) {
	char *backupdir; //백업디렉토리를 생성할 절대경로 값 저장 위한 문자열

	backupdir = (char *)malloc(sizeof(char) * 256); //절대경로 저장을 위해 메모리 동적할당

	//main 함수 : 입력받은 경로 존재, 접근 여부 확인 (별도 함수로 처리)
	backupdir = checkAccessDir(argc, argv);

	//제대로 열 수 있으면 경로 프롬프트 실행 함수에 넘겨 백업 프로그램 시작
	printPrompt(backupdir);

	return 0;
}

//================================연결리스트 관련 함수 정의부=======================================

// 연결리스트 초기화 함수
void initList (LinkedList *linkedList) {
	linkedList -> head = (Node*)malloc(sizeof(Node));
	linkedList -> tail = (Node*)malloc(sizeof(Node)); //연결리스트의 head, tail에 동적할당

	linkedList -> head -> next = linkedList -> tail;
	linkedList -> tail -> prev = linkedList -> head; //head와 tail 연결

	linkedList -> tail -> next = NULL;
	linkedList -> head -> prev = NULL;//tail->next와 head->prev 초기화

	linkedList -> fileCnt = 0; //백업할 파일 개수 카운트 0으로 초기화
}

// make node function
Node *makeNode (char *fileName) {
	Node *newNode;

	newNode = (Node *)malloc(sizeof(Node));
	strcpy(newNode -> fileName, fileName);

	newNode -> next = NULL;
	newNode -> prev = NULL;

	return newNode;
}

//연결리스트에서 노드 찾는 함수
Node *findNode (LinkedList *linkedList, char *fileName) {
	Node *curr; //리스트 순회하며 찾는 노드와 같은 노드 반환

	curr = linkedList -> head -> next; //curr노드 : 연결리스트의 head의 next위치로 초기화

	while (curr -> next != NULL) { //curr의 next가 연결리스트의 tail이 될 때까지 순회
		if (!strcmp(curr->fileName, fileName)) { //비교하여 같으면 curr 리턴
			return curr;
		}

		curr = curr -> next;
	}
}

//연결리스트에 노드를 추가하는 함수
void addNode (LinkedList *linkedList, Node *node) {
	//리스트가 비어있는 경우
	if (linkedList -> head -> next == linkedList -> tail) { //head -> next  == tail
		linkedList -> head -> next = node; //head --> node
		node -> next = linkedList -> tail; //노드 next = tail
		node -> prev = linkedList -> head; //노드 prev = head
		linkedList -> tail -> prev = node;
	}

	//리스트가 비어있지 않은 경우
	else {
		linkedList -> tail -> prev -> next = node; //node : 추가, tail->prev : 기존노드
		node -> next = linkedList -> tail;

		node -> prev = linkedList -> tail -> prev;
		linkedList -> tail -> prev = node;
	}

	linkedList -> fileCnt++; //연결리스트에 저장된 백업해야할 파일의 개수 카운트 ++
}

//연결리스트에 노드를 삭제하는 함수
void removeNode (LinkedList *linkedList, char *fileName) {
	Node *curr; //리스트를 순회하며 찾는 노드와 같은 노드 삭제

	curr = linkedList -> head -> next; // curr노드의 위치를 head 노드의 next 위치로 초기화시키는 작업

	while (curr -> next != NULL) {
		if (!strcmp(curr -> fileName, fileName)) {
			curr -> prev -> next = curr -> next; //curr의 이전노드의 next를 curr의 다음 노드에 연결
			curr -> next -> prev = curr -> prev; //curr의 다음노드의 prev를 curr의 이전 노드에 연결

			linkedList -> fileCnt--; //연결리스트에 연결되어있는 노드의 수 -1
		}

		curr = curr -> next;
	} 
}

//add new file into backup list
void addFileToList (char *dirName, Node *node) {
	FILE *fptr;

	fptr = fopen("backupList.list", "a");
	fprintf(fptr, "%s", node -> fileName);

	fclose(fptr);
}

// list 기능을 위해 모든 백업 리스트를 출력하는 함수
void listFunc(LinkedList *linkedList) {
	Node *curr;

	curr = linkedList -> head -> next; //curr 노드 초기화

	while (curr -> next != NULL) {
		printf("%s \t %d\n", curr -> fileName, curr -> filePeriod);

		curr = curr -> next;
	}

	return;
}

//================================디렉토리 관련 함수 정의부===================================

//main에서 인자로 입력받은 절대경로의 접근 권한 확인 함수
char *checkAccessDir(int argc, char **argv) {
	static char backupdir[256] = ""; //반환할 경로 저장할 배열
	struct dirent *dir; //dirent 구조체
	struct stat sb; //stat 구조체
	DIR *dirptr = NULL; //dirent 포인터
	char tmpdir[256] = ""; //임시로 현재 디렉토리를 가져와 저장해둘 배열

	int mode = R_OK & W_OK; // 읽기, 쓰기 가능

	realpath(argv[1], tmpdir); //상대경로를 절대경로로 변경하여 tmpdir에 저장
	stat(tmpdir, &sb);

	//인자를 2개 넘겨받은 경우
	if (argc == 2) {
		//성공적으로 접근 가능한 경우
		if (access(tmpdir, mode) == 0) {
			sprintf(backupdir,"%s/backup_directory", tmpdir); //백업디렉토리 명까지 backupdir에 저장
			mkdir(backupdir, 0775); //백업디렉토리 생성

			return backupdir; // 접근권한 있으면 backupdir 리턴
		}

		//디렉토리를 찾을 수 없는 경우
		else if ((dirptr = opendir(tmpdir)) == NULL) {
			printf("Usage : ./ssu_backup <dirname>\n");
			exit(0);
		}

		//인자가 디렉토리 파일이 아닌 경우
		else if (S_ISDIR(sb.st_mode) == -1) {
			printf("Usage : ./ssu_backup <dirname>\n");
			exit(0);
		}

		//접근 권한이 없을 경우	
		else if (access(tmpdir, mode) == -1) {
			printf("Usage : ./ssu_backup <dirname>\n");
			exit(0); //프로그램 종료
		}
	}

	//인자로 전달받은 경로가 없을 경우
	else if (argc == 1) {
		getcwd(tmpdir,256); //현재 경로 가져온 뒤
		sprintf(backupdir, "%s/backup_directory", tmpdir);

		mkdir(backupdir, 0755); //실행파일이 있는 곳(현재 디렉토리)에 백업 디렉토리 생성
		return backupdir;
	}

	//인자가 2개 이상인 경우
	else if (argc > 2) {
		printf("Usage : ./ssu_backup <dirname>\n");
		exit(0);
	}
}

//'학번>' 프롬프트를 출력하는 함수
void printPrompt (char *absPath) {
	char userInput[300] = ""; // 사용자로부터 명령어와 filename, period를 입력받기 위한 문자열배열
	char *fileName; // 파일의 절대경로 저장 배열
	int period; //파일의 백업주기 저장 변수
	LinkedList linkedList;
	//BackupContentList backupContentList;

	initList(&linkedList);
	//initList(&backupContentList);

	while(1) {
		printf("20193017>");
		fgets(userInput, 256, stdin);

		userInput[strlen(userInput) -1] = '\0';

		//토큰 분리해서 명령어 호출 부분과 파일명, 백업주기 해당 기능 수행에 넘기는 작업 진행
		char *order = strtok(userInput, " "); //공백자 기준으로 문자열 자름

		while (order != NULL) {

			//add <파일명> [백업주기]
			if (strcmp(order, "add") == 0) {
				fileName = strtok(NULL, " "); //다음 문자열 잘라 fileName에 저장
				period = atoi(strtok(NULL, " ")); // 다음 문자열 잘라 chPeriod에 저장

				//add 기능 담당하는 함수에 filepath와 period 인자로 넘겨주기
				addFunc(&linkedList, absPath, fileName, period); 
			}

			//remove <파일명>
			else if (strcmp(order, "remove") == 0) {
				fileName = strtok(NULL, " "); //다음 문자열 잘라 fileName에 저장

				//remove 기능 담당하는 함수에 filepath 넘기기
				removeFunc(&linkedList, absPath, fileName);
			}

			//compare <파일명1> <파일명2>
			else if (strcmp(order, "compare") == 0) {
				int paramCnt = 0; //fileName1, fileName2 

				fileName = strtok(NULL, " "); //다음 문자열 잘라 fileName에 저장
				if (strlen(fileName) != 0) {
					paramCnt++;
				}

				char *fileName2 = strtok(NULL, " "); // 다음 문자열 잘라 fileName2에 저장
				if (strlen(fileName2) != 0) {
					paramCnt++;
				}

				if (paramCnt != 2) {
					printf("not 2 paremeter.\n");
					exit(1);
				}

				//compare 기능 담당하는 함수에 filePath, filePath2 넘기기
				compareFunc(fileName, fileName2);
			}

			//recover <파일명>
			else if (strcmp(order, "recover") == 0) {
				fileName = strtok(NULL, " "); //다음 문자열 잘라 fileName에 저장

				//recover 기능 담당하는 함수에 filePath 넘기기
				recoverFunc(&linkedList, absPath, fileName);
			}

			//list 명령어
			else if (strcmp(order, "list") == 0) {

				listFunc(&linkedList);
			}

			//ls 명령어
			else if (strcmp(order, "ls") == 0) {
				/*char *dirpath = strtok(NULL, " "); //next token
				  char *lsOrder = "ls ";

				  strcat(lsOrder, dirpath); //ls dirname
				  system(lsOrder);*/
				system(userInput);
				exit(0);
			}

			//vi 명령어
			else if (strcmp(order, "vi") == 0) {
				/*char *dirpath = strtok(NULL, " "); //next token
				  char *viOrder = "vi ";

				  strcat(viOrder, dirpath);
				  system(viOrder);*/
				system(userInput);
				exit(0);
			}

			//vim 명령어
			else if (strcmp(order, "vim") == 0) {
				/*char *dirpath = strtok(NULL, " "); //next token
				  char *vimOrder = "vim ";

				  strcat(vimOrder, dirpath);
				  system(vimOrder);*/
				system(userInput);
				exit(0);
			}

			//exit 명령어
			else if (strcmp (order, "exit") == 0) {
				exit(0);
				return;
			}

			//another order
			else {
				printf("입력한 명령어가 존재하지 않습니다.\n");
			}

		}
	}
}

//연결리스트(백업리스트)에 저장되어있는 백업할 파일(노드)의 개수를 세는 함수
int calcFileCnt (LinkedList *linkedList) {
	int cnt = 0; //노드 개수 세는 변수
	DIR *dirptr; //디렉터리형 포인터

	char buf[300] = ""; //작업 디렉토리의 이름 저장하기 위한 배열
	struct dirent *dir; //dirent 구조체

	getcwd(buf, 300); // 현재 작업디렉터리의 이름 size만큼 buf에 복사

	if ((dirptr = opendir(buf)) == NULL) {
		fprintf(stderr, "opendir error\n");

		return -1;
	}

	while ((dir = readdir(dirptr)) != NULL) {
		for (int i = 0; i < strlen(dir -> d_name); i++) {

			//	cnt++; break;
		}
	}
}

//add 기능을 수행하는 함수
void addFunc(LinkedList *linkedList, char *dirPath, char *fileName, int period) {
	FILE *fptr = NULL;
	DIR *dptr = NULL;
	char buf[256];
	struct tm *loctm;
	time_t timer;
	char logmsg[1024] = "";
	int year, mon, day, hour, min, sec;

	Node *newFile; //추가되는 노드
	char *thread_name; //파일명으로 스레드 이름을 저장해둠
	int thr_id; //pthread_create() 리턴값 저장 변수
	int status; 
	int a =100;

	char *fulltime = (char *)malloc(sizeof(char) * 100);

	strcpy(thread_name, fileName);
	memset(logmsg, 0, 1024);
	timer = time(NULL);
	loctm = localtime(&timer);

	year = loctm -> tm_year + 1900;
	mon = loctm -> tm_mon +1;
	day = loctm -> tm_mday;
	hour = loctm -> tm_hour;
	min = loctm -> tm_min;
	sec = loctm -> tm_mday;

	sprintf(fulltime, "[%d%d%d %d%d%d] ", year, mon, day, hour, min, sec);

	if ((dptr = opendir(dirPath)) == NULL) {
		printf("opendir() error\n");
		exit(1);
	}

	newFile = makeNode(fileName);//노드 만들고
	addNode(linkedList, newFile);//리스트에 노드 추가
	addFileToList(dirPath, newFile);

	//노드 구조체 멤버 값 설정
	strcpy(newFile -> absPath, dirPath);
	strcpy(newFile -> fileName, fileName);
	newFile -> filePeriod =  period;

	//스레드 생성부
	thr_id = pthread_create(&p_thread[ncount], NULL, thread_function, (void *)thread_name);
	if (thr_id < 0) {
		perror("thread create error: ");
		exit(0);
	}

	newFile -> tid = &p_thread[ncount]; //구조체의 스레드 식별자 변수에 생성된 스레드 식별자 저장
	
	thread_function((void *)thread_name); //add()함수에서도 스레드에서 돌아가고 있는 동일한 함수 실행

	for (int i = 0; i < ncount; i++) {
		pthread_join(p_thread[i], (void **)&status); //스레드의 종료를 기다림
	}

	ncount++; //전역변수로 설정된 스레드 식별자 선언 개수를 +1

	//sprintf(logmsg, "%s %s%s added\n", fulltime, dirPath, fileName);
	//write_log(logmsg);
}

//remove 기능을 수행하는 함수
void removeFunc(LinkedList *linkedList, char *dirPath, char *fileName) {
	Node *removeFile;

	pthread_t rm_thread; //삭제할 스레드 식별자
	FILE *fptr;
	DIR *dirptr;//dirent구조체 포인터
	struct dirent *entry = NULL;//dirent구조체
	char cmp_fileName[256] =""; //strstr()의 리턴 값을 저장할 배열
	int removeFileCnt = 0; //백업디렉토리에서 삭제할 파일의 수 카운트
	static int retval = 999; //스레드 종료 시 반환 값 설정을 위한 변수

	struct tm *loctm;
	time_t timer;

	char logmsg[1024] = "";
	char *fulltime;
	//char *abspath;
	int year, mon, day, hour, min, sec;

	fulltime = (char *)malloc(sizeof(char) * 100);
	//abspath = (char *)malloc(sizeof(char) * 256);

	if (fileName == NULL) { //FILENAME을 입력하지 않은 경우
		printf("파일명을 입력하세요.\n");
		exit(0);
	}

	removeFile = findNode(linkedList, fileName); //연결리스트에서 파일명의 노드를 찾아 리턴받음
	
	if (removeFile == NULL) { //백업 중단할 파일이 리스트에 존재하지 않는 경우
		printf("백업을 중단할 파일이 백업 리스트에 존재하지 않습니다.\n");
		exit(0);
	}

	timer = time(NULL);
	loctm = localtime(&timer);

	dirptr = opendir(dirPath);//함수 호출 시 인자로 전달받은 백업디렉토리를 open
	if (dirptr == NULL) {
		printf("%s 디렉토리를 열 수 없습니다.\n", dirPath);
		exit(0);
	}

	else {
		while((entry = readdir(dirptr)) != NULL) {//백업디렉토리의 모든 파일의 순회를 마칠 때까지

			memset(cmp_fileName, 0, 256);//파일 순회마다 cmp_fileName초기화

			if (strstr(entry->d_name, fileName) != NULL) {
				removeNode(linkedList, entry->d_name); //백업파일을 모두 삭제
				removeFileCnt++;

				//pthread_exit((void *)&retval);
			}
		}

		pthread_cancel(removeFile -> tid); //삭제하고자하는 파일의 스레드 종료

		//노드 삭제 수행 시간 계산
		year = loctm -> tm_year + 1900;
		mon = loctm -> tm_mon +1;
		day = loctm -> tm_mday;
		hour = loctm -> tm_hour;
		min = loctm -> tm_min;
		sec = loctm -> tm_sec;
		
		//파일 삭제 수행을 성공적으로 완료하면 deleted로그 작성
		sprintf(fulltime, "[%d%d%d %d%d%d] ", year, mon, day, hour, min, sec);
		sprintf(logmsg, "%s %s%s deleted\n", fulltime, dirPath, fileName);
		write_log(logmsg);
	}

	//fclose(fptr);
	closedir(dirptr);
}

//compare 기능을 수행하는 함수
void compareFunc(char *fileName1, char *fileName2) {
	FILE *file1;
	struct stat buf1;
	//struct tm *mtime1;
	char *mtime1;
	size_t size1; //fileName1의 내용 담는 file, stat, tm, size

	FILE *file2;
	struct stat buf2;
	//struct tm *mtime2;
	char *mtime2;
	size_t size2; //fileName2의 내용 담은 file, stat, tm, size

	stat(fileName1, &buf1);
	stat(fileName2, &buf2);

	mtime1 = ctime(&buf1.st_mtime); //fileName1의 mtime 측정
	mtime2 = ctime(&buf2.st_mtime); //fileName2의 mtime 측정

	//size1 = getFileSize(fileName1);
	//size2 = getFileSize(fileName2);

	//파일이 존재하지 않을 경우
	if (!(file1 = fopen(fileName1, "r")) && !(file2 = fopen(fileName2, "r"))) {
		fclose(file1);
		fclose(file2);

		printf("입력한 파일이 존재하지 않습니다.\n");
		exit(1);
	}

	//일반 파일이 아닌 경우
	if (!S_ISREG(buf1.st_mode) && !S_ISREG(buf2.st_mode)) {
		printf("일반 파일이 아닙니다.\n");
		exit(1);
	}

	//mtime과 파일크기 비교
	if (strcmp(mtime1, mtime2) == 0) {
		if (buf1.st_size == buf2.st_size) {
			printf("FILENAME1과 FILENAME2은 동일한 파일입니다.\n");
		}

		else {
			printf("FILENAME1 -> mtime : %s , 파일크기 : %d \n", mtime1, (int)(buf1.st_size));
			printf("FILENAME2 -> mtime : %s , 파일크기 : %d \n", mtime2, (int)(buf2.st_size));
		}
	}
}

//파일 크기를 가져오기 위한 함수 (compare) 명령어에서 사용
static size_t getFileSize(const char *fileName) {
	struct stat sb;

	if (stat(fileName, &sb) != 0) {
		fprintf(stderr, "'stat' failed for '%s' : \n", fileName);
		exit(1);
	}

	return sb.st_size;
}

//recover 기능을 수행하는 함수
void recoverFunc(LinkedList *linkedList, char *dirPath, char *fileName) {
	struct stat sb;
	struct stat tmpsb; //순회하는 백업본 파일 정보 저장 구조체

	DIR *dirptr = NULL;//dirent구조체 포인터
	struct dirent *entry = NULL;//dirent 구조체
	FILE *fptr = NULL; //파일 오픈을 위한 파일 포인터
	FILE *src = NULL; //복사할 파일 포인터
	FILE *dest = NULL; //붙여넣기할 파일 포인터

	int rcvFileCnt = 0;//recover 파일 카운터 변수
	char *cmp_fileName; //strstr() 리턴 값 저장 위한 배열
	char tmpmem[400] = ""; //2차원 포인터에 저장할 값을 임의로 담아두는 배열
	char backuptime[13]; //파일 백업 수행시간만 분리해 저장하는 배열
	char fileSize[100] ="";//파일의 크기 저장을 위한 배열
	int size = 0; //ftell() 함수의 리턴 값을 저장하는 변수
	char **rcvFileList; //백업파일들을 담아놓기 위한 2차원 포인터

	struct tm *loctm;
	time_t timer;
	int year, mon, day, hour, min, sec;
	int backupFileCnt = 0; //백업된 파일의 개수
	int userChoice = 0;
	//char logmsg[1024] = "";

	Node *recoverFile; //복구할 파일의 정보를 가져오기 위한 구조체
	recoverFile = (Node *)malloc(sizeof(Node)); //recoverFile 노드 동적할당
	recoverFile = findNode(linkedList, fileName); //입력받은 노드의 존재여부 확인
	
	if (recoverFile == NULL) { //변경할 파일이 존재하지 않는 경우
		printf("변경할 파일이 존재하지 않습니다.\n");
		exit(1);
	}

	char *fulltime = (char *)malloc(sizeof(char)*100);
	char *recovertime = (char *)malloc(sizeof(char)*100); // 백업 수행시간을 정렬하기 위해

	stat(fileName, &sb); //파일 정보 저장

	timer = time(NULL);
	loctm = localtime(&timer);

	year = loctm -> tm_year + 1900;
	mon = loctm -> tm_mon +1;
	day = loctm -> tm_mday;
	hour = loctm -> tm_hour;
	min = loctm -> tm_min;
	sec = loctm -> tm_sec;

	dirptr = opendir(dirPath);
	if (dirptr == NULL) {
		printf("백업 디렉토리를 열 수 없습니다.\n");
		exit(0);
	}

	else {
		while(entry = readdir(dirptr) != NULL) {//파일 개수 카운트를 위한 반복문
			memset(cmp_fileName, 0, 256);//cmp_fileName 수행시마다 배열 초기화

			if ((cmp_fileName = strstr(entry->d_name, recoverFile -> fileName)) != NULL) {
				rcvFileCnt++; //사용자가 입력한 파일의 백업본 개수 카운트
			}	
		}

		rcvFileList = (char**)malloc(sizeof(char *) * rcvFileCnt); //백업파일 개수만큼 동적할당

		for (int i = 0; i < rcvFileCnt; i++) {
			rcvFileList[i] = (char*)malloc(sizeof(char) * 400);
		}

		while((entry = readdir(dirptr)) != NULL) {//출력 내용 저장을 위한 반복문
			memset(cmp_fileName, 0, 256);
			memset(backuptime, 0, 13);
			memset(tmpmem, 0, 400);
			size = 0;
			fptr = NULL;
			int i = 1; // 0 : exit
			int nameptr = 0; //백업수행시간의 포인터를 가져오기위한 변수

			if ((cmp_fileName = strstr(entry -> d_name, recoverFile -> fileName)) != NULL) {
				stat(entry -> d_name, &tmpsb);
				size = tmpsb.st_size; //파일 크기 저장
			
				//fptr = fopen(entry -> d_name, "r"); //entry -> d_name 파일을 읽기모드로 open
				//fseek(fptr, 0, SEEK_END); //파일 포인터를 파일의 끝으로 이동
				//size = ftell(fptr); //파일 포인터의 현재 위치 얻음
				sprintf(fileSize, "%dbytes", size);

				nameptr = (int)(entry -> d_reclen + 1);//파일명 중 백업시간 기록 시작 위치 저장 
				strcpy(backuptime, &(entry->d_name[nameptr]));//backuptime에 백업시간 저장

				sprintf(tmpmem, "%d. %s \t %s", i, backuptime, fileSize);//순번, 백업시간, 파일 크기 저장
				rcvFileList[i] = tmpmem;
				i++;
			}

		}

		//sprintf(fulltime, "[%d%d%d %d%d%d] ", year, mon, day hour, min, sec);
		//sprintf(recovertime, "%d%d%d%d%d%d", year, mon, day, hour, min, sec);
		//sprintf(logmsg, "%s %s%s_%s generated", fulltime, dirName, fileName, recovertime);
		//write_log(logmsg);

	}

	//순번 파일명 파일크기 출력부
	printf("0. exit\n");
	for (int i = 1; i < rcvFileCnt; i++) {
		printf("%s", rcvFileList[i]); //순번. 파일명 파일크기 출력
	}

	printf("choose file to recover : ");
	scanf("%d", &userChoice);

	for (int i = 1; i < rcvFileCnt; i++) { //인덱스 1부터 파일의 백업본이 저장되어 있음
		if (userChoice == 0) {
			exit(0);
			return;//0을 입력한 경우 프롬프트로 제어 이동
		}

		else if (userChoice == i) {
			//선택한 파일으로 fileName_recover 파일 복구
			char *recovered;
			//char *tmprcv1; //사용자가 선택한 파일명에 접근하기 위해 사용되는 임시 배열
			//char *tmprcv2;

			char *buffer; //파일의 내용을 읽어와 쓰는 과정 중 잠시 사용되는 버퍼
			int cnt = 1; 
			pthread_cancel(recoverFile -> tid); //백업수행 종료 후 복구 진행을 위해 백업수행 스레드 종료
			while ((entry = readdir(dirptr)) != NULL) { //찾는 파일을 가져옴
				if (cnt == i) { //사용자가 입력한 순번과 순서대로 파일을 읽어들이는 파일 개수가 일치할 경우 
					sprintf(recovered, "%s_recover", recoverFile -> fileName);
					strcpy(entry->d_name, recovered);
					break;
				}

				cnt++; //파일을 하나씩 읽을 때마다 카운트 수 +1
			}

			/*
			strcpy(tmprcv1,rcvFileList[i]); //사용자가 선택한 파일의 내용을 저장해둠
			sprintf(recovered, "%s_recover.txt", fileName); //수정할 것

			src = fopen(entry -> d_name, "r"); //선택한 파일로 접근하도록 수정할 것(dirent)
			dest = fopen(recovered, "w+");

			while (feof(src) == 0) {
				cnt = fread(buffer, sizeof(char), strlen(), src);
				fwrite(buffer, sizeof(char), cnt, dest); //읽어온 내용 recover파일에 붙여넣기
			}
			*/
		}
	}
	
	fclose(fptr);
	//fclose(src);
	//fclose(dest);
	closedir(dirptr);
}


//ls function
void lsFunc() {
	DIR *dirptr = NULL;
	struct dirent *dir = NULL;
	int printCnt = 0;

	char *cwd = (char*)malloc(sizeof(char) * 256);

	getcwd(cwd, 256); //get current working directory

	if ((dirptr = opendir(cwd)) == NULL) {
		printf("ls function error : current directory cannot open\n");
		exit(1);
	}

	while((dir = readdir(dirptr)) != NULL) {
		printf("%s\t", dir->d_name);
	}

	closedir(dirptr); 
}

//============================= << logfile.log >> ================================
void write_log(char *msg) {
	FILE *fptr;
	char logMsg[1024];

	fptr = fopen("logfile.log", "w");

	if (fptr != NULL) {
		fprintf(fptr, "%s\n", logMsg);
	}

	fclose(fptr);
}

//=========================== << pthread function >> ===================================
void *thread_function (void *addFile) {
	Node *newNode = (Node *)addFile;

	struct stat statbuffer;//stat구조체
	struct stat copystat;
	FILE *addfptr = NULL; //add한 파일 포인터
	FILE *backupfptr = NULL; //backup한 파일 포인터
	DIR *dirptr; //dirent 포인터
	struct dirent *dir; //dirent 구조체
	
	time_t timer;
	struct tm *loctm;
	char *backupFile; //백업된 파일의 이름을 '_년월일시분초'를 추가해 저장하는 배열

	char *tmpbuf; //파일 내용 읽어와 쓰는 작업 중 사용하는 임시 버퍼
	char justFileName[256] = ""; // 파일명
	char fullFilePath[256] = ""; // 절대경로를 포함한 파일명
	char logmsg[1024] = ""; 

	int year, mon, day, hour, min, sec;
	char *fulltime;

	pid_t pid; //프로세스 식별자
	pthread_t tid; //스레드 식별자
	char* thread_name = (char*)addNode;
	backupFile = (char *)malloc(sizeof(char) *256);

	//justFileName을 알아오는 작업 구현부

	pid = getpid();
	tid = pthread_self();

	newNode -> pid = pid;
	newNode -> tid = tid; 
	//newNode -> filePeriod = period;
	//newNode -> fileName = addFile;
	 
	int dirResult = mkdir("generatedPath", 0755); // 백업이 진행된 파일들을 저장할 디렉토리 생성
	if (dirResult != 0) {
		printf("mkdir error\n");
		exit(1);
	}

	fulltime = (char *)malloc(sizeof(char) *100);

	if ((timer = time(NULL)) == -1) { //초 단위로
		perror("time() call error");
		exit(1);
	}

	if ((loctm = localtime(&timer)) == NULL) { //시간을 분리하여 tm 구조체에 넣음
		perror("localtime() call error");
		exit(1);
	}

	memset(logmsg, 0, 1024);

	year = loctm -> tm_year + 1900;
	mon = loctm -> tm_mon +1;
	day = loctm -> tm_mday;
	hour = loctm -> tm_hour;
	min = loctm -> tm_min;
	sec = loctm -> tm_sec;
	sprintf(fulltime, "[%d%d%d %d%d%d] ", year, mon, day, hour, min, sec); // 로그파일 작성 ex) [190311 153320] 

	pthread_mutex_lock(&mutex); //스레드 동기화 lock
	sprintf(logmsg, "%s %s added\n", fulltime, fullFilePath);
	write_log(logmsg);
	pthread_mutex_unlock(&mutex); //스레드 동기화 unlock

	sleep(newNode -> filePeriod);// 백업 수행 주기만큼 시간 지연
	stat(newNode -> fileName, &statbuffer);

	while(1) { //해당 노드가 pthread_exit() 까지 반복
		stat(newNode -> fileName, &statbuffer); //추가한 파일의 정보를 읽어올 수 있도록
		tmpbuf = (char *)malloc(sizeof(char) * statbuffer.st_size); //파일 크기만큼 tmpbuf 동적할당
	
		addfptr = fopen(newNode -> fileName, "r"); //추가한 파일을 읽기 모드로 fopen
		if (addfptr == NULL) {
			fprintf(stderr, "fopen() error\n");
			exit(1);
		}

		memset(fulltime, 0, 100); //백업 수행 시간 저장하는 배열 초기화
		memset(logmsg, 0, 1024); //로그 메시지 저장 배열 초기화
		
		//백업 수행시간 측정하여 fulltime에 저장
		year = loctm -> tm_year + 1900;
		mon = loctm -> tm_mon +1;
		day = loctm -> tm_mday;
		hour = loctm -> tm_hour;
		min = loctm -> tm_min;
		sec = loctm -> tm_sec;
		sprintf(fulltime, "[%d%d%d %d%d%d] ", year, mon, day, hour, min, sec); //fulltime 저장
		sprintf(backupFile, "%s_%d%d%d%d%d%d", justFileName, year, mon, day, hour, min, sec); //백업된 파일 만드는 부분

		backupfptr = fopen(backupFile, "w+"); //파일이 존재하지 않을 경우 빈 파일을 새로 생성(백업할 파일)
		if (backupfptr == NULL) {
			fprintf(stderr, "backupfptr : fopen() error\n");
			exit(1);
		}

		int count = fread(tmpbuf, sizeof(char), statbuffer.st_size -1 , addfptr); //마지막 null 제외한 파일 내용 읽어옴
		fwrite(tmpbuf, sizeof(char), count, backupfptr); //읽어들인 내용을 파일에 씀

		sprintf(logmsg, "%s %s generated", fulltime, fullFilePath);
		write_log(logmsg); //백업 후 로그 작성
	}

	fclose(addfptr);
	fclose(backupfptr);
	closedir(dirptr);

}
