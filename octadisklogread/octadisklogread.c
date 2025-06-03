/* octadisklogread */
/* gcc -o octadisklogread octadisklogread.c -lm */
/* 2018 Jul. 27 by Y.Yonekura */

/* pick-up show_status and show_alarm error flag from octadisk log file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


int main(int argc, char *argv[])
{

  int flag;
  int i;
  char wild[100];
  char buff[512];

  FILE *fpin;

	if(argc != 2){
		fprintf(stderr, "octadisk log-file name: ");
		fscanf(stdin, "%s", wild);
		}
	else{
		sscanf(argv[1], "%s", wild);
		}
	

	if(NULL==(fpin=fopen(wild,"rt"))){
	  fprintf(stderr,"can not open log-file %s.\n", wild);
		exit(-1);
		}
	
	for(;;){
	  if(fgets(buff, 512, fpin)==NULL) break;

	  if(strstr (buff, "!show_status?0:") != NULL){
	    if(strlen(buff) == 17){
	      	      printf("%lu %s", strlen(buff), buff);


	      if(fgets(buff, 512, fpin)==NULL) break;
	      if(fgets(buff, 512, fpin)==NULL) break;
	      if(fgets(buff, 512, fpin)==NULL) break;
	  	  printf("show_status:now:%lu %s", strlen(buff), buff);
	      flag = 0;
	      for(i=0;i<44;i++){
		if((buff[i]!='0')&&(buff[i]!='x')&&(buff[i]!=':')&&(buff[i]!=';')&&(buff[i]!=' ')){
		  flag += 1;
		  break;
		}
	      }
	      if(flag!=0) printf("%d %c show_status:now:%lu %s", i, buff[i],strlen(buff), buff);

	      if(fgets(buff, 512, fpin)==NULL) break;
	      if(fgets(buff, 512, fpin)==NULL) break;
	      if(fgets(buff, 512, fpin)==NULL) break;
	      	  printf("show_status:past%lu %s", strlen(buff), buff);
	      flag = 0;
	      for(i=0;i<44;i++){
		if((buff[i]!='0')&&(buff[i]!='x')&&(buff[i]!=':')&&(buff[i]!=';')&&(buff[i]!=' ')){
		  flag += 1;
		  break;
		}
	      }
	      if(flag!=0) printf("%d %c show_status:past%lu %s", i, buff[i],strlen(buff), buff);



	    }
	  }
	}

	fclose(fpin);


  return 0;
}
