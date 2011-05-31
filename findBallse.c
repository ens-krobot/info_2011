/*
 * findBallse.c
 * ------------
 * Copyright : (c) 2008, Xavier Lagorce <lagorce@crans.org>
 *             (c) 2011, Jeremie Dimino <jeremie@dimino.org>
 * Licence   : BSD3
 *
 * This file is a part of [kro]bot.
 */

#include "cv.h"
#include "highgui.h"
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>

/* +-----------------------------------------------------------------+
   | Parameters                                                      |
   +-----------------------------------------------------------------+ */

/* The parameters of the color to find in images. */
struct color_params {
  int centH, radH, minS, maxS, minV, maxV;
  int threshold;
  int minCont, maxCont;
};

/* Global parameters. */
struct color_params params;

/* Load the source image. HighGUI use. */
IplImage *image01 = 0, *image02 = 0, *image03 = 0, *imCont = 0, *imFill = 0, *imHSV = 0;

/* Whether to display the result on the screen or not using HighGUI. */
int display = 0;

CvMat* homography;

/* +-----------------------------------------------------------------+
   | Config file parsing                                             |
   +-----------------------------------------------------------------+ */

/* Parse the configuration file and store the result into params. */
void parse_config(char *file_name)
{
  FILE *fp = fopen(file_name, "r");

  char *line = NULL;
  size_t line_len = 0;
  char key[1024];
  int value;

  while (getline(&line, &line_len, fp) != -1) {
    char *p;

    /* Skip blanks at the beginning of the line. */
    for (p = line; *p && isblank(*p); p++);

    /* Skip empty lines and comments. */
    if (*p == 0 || *p == '#') continue;

    /* Check that key won't overflow. */
    if (strlen(line) >= 1024) errx(2, "line too long in configuration file");

    /* Parse the line. */
    if (sscanf(p, "%s = %d \n", &key, &value) != 2) errx(2, "invalid configuration file");

    /* Assign the value to the right parameter. */
    if (strcmp(key, "centH") == 0)
      params.centH = value;
    else if (strcmp(key, "radH") == 0)
      params.radH = value;
    else if (strcmp(key, "minS") == 0)
      params.minS = value;
    else if (strcmp(key, "maxS") == 0)
      params.maxS = value;
    else if (strcmp(key, "minV") == 0)
      params.minV = value;
    else if (strcmp(key, "maxV") == 0)
      params.maxV = value;
    else if (strcmp(key, "threshold") == 0)
      params.threshold = value;
    else if (strcmp(key, "minCont") == 0)
      params.minCont = value;
    else if (strcmp(key, "maxCont") == 0)
      params.maxCont = value;
    else
      errx(2, "invalid key in configuration file: '%s'", key);
  }

  if (line) free(line);
  fclose(fp);
}

/* +-----------------------------------------------------------------+
   | Image processing                                                |
   +-----------------------------------------------------------------+ */

int in_radius(int val, int center, int radius, int max)
{
  int d;
  d = abs(val - center);
  return (d <= radius) || (max - d <= radius);
}

// This function the balls,
void process_image()
{
  CvMemStorage* stor;
  CvSeq* cont;
  CvBox2D32f* box;
  CvPoint* PointArray;
  CvPoint2D32f* PointArray2D32f;
  int i, j, meanRad;

  CvMat* src;
  CvMat* dst;

  // Changement d'espace de couleur
  cvCvtColor(image01, imHSV, CV_BGR2HSV);

  // Génération de l'image avec les résultats
  if (display != 0)
    {
      imCont = cvCloneImage(image01);
      cvZero(imCont);
      // On fixe la ROI de l'image pour ne pas dessiner à l'extérieur
      imCont->roi = (_IplROI*)malloc(sizeof(IplROI));
      imCont->roi->coi = 0;
      imCont->roi->xOffset = 0;
      imCont->roi->yOffset = 0;
      imCont->roi->width = imCont->width;
      imCont->roi->height = imCont->height;
    }

  // On fait le traitement pour chaque couleur de balle
  imFill = cvCloneImage(image01);

  for (i=0 ; i < imFill->width*imFill->height; i++)
    {
      if (in_radius(imHSV->imageData[3*i], params.centH, params.radH, 180) &&
          (uchar)imHSV->imageData[3*i+1] >= params.minS && (uchar)imHSV->imageData[3*i+1] <= params.maxS &&
          (uchar)imHSV->imageData[3*i+2] >= params.minV && (uchar)imHSV->imageData[3*i+2] <= params.maxV )
        {
          // On laisse la couleur du pixel
        }
      else
        {
          // sinon on l'efface
          imFill->imageData[3*i  ] = 0;
          imFill->imageData[3*i+1] = 0;
          imFill->imageData[3*i+2] = 0;
        }
    }

  // Conversion en niveaux de gris de l'image filtrée
  cvCvtColor(imFill, image03, CV_BGR2GRAY);

  // Create the destination images
  image02 = cvCloneImage( image03 );

  // Create dynamic memory storage and sequence.
  stor = cvCreateMemStorage(0);
  cont = cvCreateSeq(CV_SEQ_ELTYPE_POINT, sizeof(CvSeq), sizeof(CvPoint) , stor);

  /* Print a separator. */
  printf("=====\n");

  if (cont) {
    // Threshold the source image. This needful for cvFindContours().
    cvThreshold( image03, image02, params.threshold, 255, CV_THRESH_BINARY );

    // Find all contours.
    cvFindContours( image02, stor, &cont, sizeof(CvContour),
                    CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0,0));

    // Clear image. IPL use.
    cvZero(image02);

    // This cycle draw all contours and approximate it by ellipses.
    for(;cont;cont = cont->h_next) {
      int i; // Indicator of cycle.
      int count = cont->total; // This is number point in contour
      CvPoint center;
      CvSize size;

      // Number point must be more than or equal to 6 (for cvFitEllipse_32f).
      if( count < 6 )
        continue;

      // Alloc memory for contour point set.
      PointArray = (CvPoint*)malloc( count*sizeof(CvPoint) );
      PointArray2D32f= (CvPoint2D32f*)malloc( count*sizeof(CvPoint2D32f) );

      // Alloc memory for ellipse data.
      box = (CvBox2D32f*)malloc(sizeof(CvBox2D32f));

      // Get contour point set.
      cvCvtSeqToArray(cont, PointArray, CV_WHOLE_SEQ);

      // Convert CvPoint set to CvBox2D32f set.
      for(i=0; i<count; i++) {
        PointArray2D32f[i].x = (float)PointArray[i].x;
        PointArray2D32f[i].y = (float)PointArray[i].y;
      }

      // Fits ellipse to current contour.
      cvFitEllipse(PointArray2D32f, count, box);

      // Convert ellipse data from float to integer representation.
      center.x = cvRound(box->center.x);
      center.y = cvRound(box->center.y);
      size.width = cvRound(box->size.width*0.5);
      size.height = cvRound(box->size.height*0.5);
      box->angle = -box->angle;

      // On ne dessine le contour et l'ellipse que si son rayon est dans les bornes
      meanRad = (size.width + size.height) / 2;

      if (meanRad >= params.minCont && meanRad <= params.maxCont) {

	// tansform matched elipsis to table coordinates
	src = cvCreateMat(1, 1, CV_32FC2);
	dst = cvCreateMat(1, 1, CV_32FC2);
	src->data.fl[0] = (float) center.x;
	dst->data.fl[1] = (float) center.y;
	cvPerspectiveTransform(point1, point2, homography);

        /* Print ellipsis parameters on stdout. */
	printf("%f %f\n", dst->data.fl[0], dst->data.fl[1]);

        if (display) {
          // Draw current contour.
          cvDrawContours(imCont,cont,CV_RGB(255,255,255),CV_RGB(255,255,255),0,1,8,cvPoint(0,0));

          // Draw ellipse.
          cvEllipse(imCont, center, size,
                    box->angle, 0, 360,
                    CV_RGB(255,0,0), 1, CV_AA, 0);
        }
      }

      // Free memory.
      free(PointArray);
      free(PointArray2D32f);
      free(box);
    }
  }

  fflush(stdout);

  // On libère la mémoire
  if (display) {
    free(imCont->roi);
    imCont->roi = NULL;
  }
  cvReleaseMemStorage(&stor);
  cvReleaseImage(&image02);
  cvReleaseImage(&imFill);
}

/* +-----------------------------------------------------------------+
   | Entry point                                                     |
   +-----------------------------------------------------------------+ */

int main( int argc, char** argv )
{
  int source, c, quit=0;
  CvCapture *capture = NULL;
  FILE *fichier;

  // Traitement des paramètres de ligne de commande
  if (argc < 2)
  {
    printf("argv[1] : chemin du fichier de paramètres\n");
    printf("argv[2] : (optionnel) si différent de 0, affiche une fenêtre\n");
    printf("argv[3] : (optionnel) numéro de la webcam à utiliser\n");
    return 1;
  }

  /* Parse configuration. */
  parse_config(argv[1]);

  display = argc >= 3 ? atoi(argv[2]) : 0;
  source = argc >= 4 ? atoi(argv[3]) : 0;

  // Ouvre la webcam
  capture = cvCaptureFromCAM(source);

  // Create window
  if (display != 0)
    cvNamedWindow("Result", 1);

  // Récupère une image de la webcam
  image01 = cvQueryFrame(capture);

  // Création de l'image en niveaux de gris à la taile de l'image prise par la webcam
  image03 = cvCreateImage(cvSize(image01->width,image01->height), IPL_DEPTH_8U, 1);

  // Même chose avec l'image pour conversion
  imHSV = cvCreateImage(cvSize(image01->width,image01->height), IPL_DEPTH_8U, 3);

  // create and initialise homograpy matrix
  homography = cvCreateMat(3, 3, CV_32F);
  homography->data.fl[0] = -0.00109182286542;
  homography->data.fl[1] = -0.0570392236114;
  homography->data.fl[2] = 7.99883031845;
  homography->data.fl[3] = -0.00867976341397;
  homography->data.fl[4] = -0.0176480710506;
  homography->data.fl[5] = 3.60714673996;
  homography->data.fl[6] = -0.000370743422536;
  homography->data.fl[7] = -0.0184959284961;
  homography->data.fl[8] = 1.0;

  while (!quit)
  {
    // Récupère une image de la webcam
    image01 = cvQueryFrame(capture);

    process_image();

    // Show the image
    if (display != 0) {
      cvShowImage("Source", image01);
      cvShowImage("Result", imCont);
    }

    // Wait for a key stroke; the same function arranges events processing
    c = (char)cvWaitKey(10);

    switch (c)
    {
      case 'q':
	quit = 1;
	break;
    }

    cvReleaseImage(&imCont);
  }

  // On release la mémoire
  cvReleaseCapture(&capture);

  cvReleaseImage(&image02);
  cvReleaseImage(&image03);
  cvReleaseImage(&imHSV);
  cvReleaseImage(&imFill);
  cvReleaseImage(&imCont);

  if (display != 0)
    cvDestroyWindow("Result");

  return 0;
}
