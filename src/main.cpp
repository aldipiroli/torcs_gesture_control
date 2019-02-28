#include <cv.h>
#include <highgui.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 


using namespace std;

int sockfd, portno;
IplImage* imgTracking;
IplImage* imgTracking_red;
IplImage* imgTracking_blue;
int lastX = -1;
int lastY = -1;

double y;
void error(const char *msg)
{
    perror(msg);
    exit(0);
}
void connect_server( int porta){
	 struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
   
    portno = porta;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
	 
}

//This function threshold the HSV image and create a binary image
IplImage* GetThresholdedImage(IplImage* imgHSV, int color){       
	IplImage* imgThresh=cvCreateImage(cvGetSize(imgHSV),IPL_DEPTH_8U, 1);
	if(color == 1)
	cvInRangeS(imgHSV, cvScalar(170,160,60), cvScalar(180,256,256), imgThresh);  // RED 
	else
	cvInRangeS(imgHSV, cvScalar(104,178,70), cvScalar(130,240,124), imgThresh);  // BLUE 
	
	return imgThresh;
}


double calibra(int x,int x_min, int x_max, int h_out, int h_min ){
double y;
	 
	 if (x_max == x_min)
		  return 0;
	
	 y=(1.0*h_out)*((1.0*(x-x_min)/(1.0*(x_max-x_min))-h_min*1.0));
	 return y;
}

void invia_messaggio(double sterzo, double acc, double freno,int marcia){
	 char stringa[20];
	 
 	sprintf(stringa, "%.2f|%.2f|%.2f|%d",sterzo,acc,freno,marcia); 
	cout<<stringa<<endl;
	int n = write(sockfd,stringa,strlen(stringa)); 
	if (n != strlen(stringa)) cout<<"Errore";
}

void trackObject(IplImage* imgThresh, char color){
// Calculate the moments of 'imgThresh'
CvMoments *moments = (CvMoments*)malloc(sizeof(CvMoments));
cvMoments(imgThresh, moments, 1);
double moment10 = cvGetSpatialMoment(moments, 1, 0);
double moment01 = cvGetSpatialMoment(moments, 0, 1);
double area = cvGetCentralMoment(moments, 0, 0);

     // if the area<1000, I consider that the there are no object in the image and it's because of the noise, the area is not zero 
if(area>1000){
        // calculate the position of the ball
int posX = moment10/area;
int posY = moment01/area;  
        
       if(lastX>=0 && lastY>=0 && posX>=0 && posY>=0)
{
// Draw a yellow line from the previous point to the current point
//cvLine(imgTracking, cvPoint(posX, posY), cvPoint(lastX, lastY), cvScalar(0,0,255), 4);
	 double acc;
	 double freno;
	 double sterzo;
	 int marcia = 0;
	 sterzo = (posX*1.0-250)/250;
	 if(posY<=250){
	 	  acc=(250-posY*1.0)/250;
		  freno=0;

	 }
	 else{
	 	acc=0;
	 	freno= (posY*1.0-250)/250;
		  if(freno < 0.25)
				marcia=-1;
	 }
		invia_messaggio(sterzo, acc, freno,5);
	  // invia_messaggio(sterzo, 1, 0,1);
		cout<<color<<" X:"<<posX<<"\t Y:"<<posY<<"\n";
}

lastX = posX;
lastY = posY;
}

free(moments);
}




int main(){

	   connect_server(2048);
      CvCapture* capture =0;       
      capture = cvCaptureFromCAM(1);
      if(!capture){
return -1;
      }
      
      IplImage* frame=0;
      frame = cvQueryFrame(capture);           
      if(!frame) return -1;
  
     //create a blank image and assigned to 'imgTracking' which has the same size of original video
     imgTracking=cvCreateImage(cvGetSize(frame),IPL_DEPTH_8U, 3);
     cvZero(imgTracking); //covert the image, 'imgTracking' to black

		cvNamedWindow("Video",CV_WINDOW_AUTOSIZE);     
		cvNamedWindow("Red", CV_WINDOW_AUTOSIZE);
		cvNamedWindow("Blue", CV_WINDOW_AUTOSIZE);
	   //cvNamedWindow("Drawing", CV_WINDOW_AUTOSIZE);
	 
      //iterate through each frames of the video     
      while(true){

            frame = cvQueryFrame(capture);           
            if(!frame) break;
            frame=cvCloneImage(frame); 
            
            cvSmooth(frame, frame, CV_GAUSSIAN,3,3); //smooth the original image using Gaussian kernel

            IplImage* imgHSV_red  = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 3); 
			   IplImage* imgHSV_blue = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 3); 
			   
            cvCvtColor(frame, imgHSV_red, CV_BGR2HSV); //Change the color format from BGR to HSV
			   cvCvtColor(frame, imgHSV_blue, CV_BGR2HSV); 
            IplImage* imgThresh_red =  GetThresholdedImage(imgHSV_red,1);
            IplImage* imgThresh_blue = GetThresholdedImage(imgHSV_blue,2);
          
            cvSmooth(imgThresh_red, imgThresh_red, CV_GAUSSIAN,3,3); //smooth the binary image using Gaussian kernel
			   cvSmooth(imgThresh_blue, imgThresh_blue, CV_GAUSSIAN,3,3); 

			  trackObject(imgThresh_red,'R');
			  trackObject(imgThresh_blue,'B');

           // Add the tracking image and the frame
           //cvAdd(imgTracking_red, imgThresh, imgTracking_red);

			  cvShowImage("Video", frame);
           cvShowImage("Red", imgThresh_red);  
			  cvShowImage("Blue", imgThresh_blue);
			 
           
           //Clean up used images
           cvReleaseImage(&imgHSV_red);
           cvReleaseImage(&imgHSV_blue);
           cvReleaseImage(&imgThresh_red);          
			  cvReleaseImage(&imgThresh_blue);  
           cvReleaseImage(&frame);

            //Wait 10mS
            int c = cvWaitKey(10);
            //If 'ESC' is pressed, break the loop
            if((char)c==27 ) break;      
      }

      cvDestroyAllWindows() ;
      cvReleaseImage(&imgTracking);
      cvReleaseCapture(&capture);     

      return 0;
}
