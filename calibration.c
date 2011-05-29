/*
 * calibration.c
 * -------------
 * Copyright : (c) 2011, Jeremie Dimino <jeremie@dimino.org>
 * Licence   : BSD3
 *
 * This file is a part of [kro]bot.
 */

/* Calibrate the webcam.

   Usage: calibrate [display] [source]
*/

#include <cv.h>
#include <highgui.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  /* Whether to display the result. */
  int display = argc >= 2 ? atoi(argv[1]) : 0;

  /* The source webcam. */
  int source = argc >= 3 ? atoi(argv[2]) : 0;

  /* Open the webcam. */
  CvCapture *capture = cvCaptureFromCAM(source);

  /* Query one frame from the webcam. */
  IplImage *image = cvQueryFrame(capture);

  /* Create the HSV image. */
  IplImage *hsv_image = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 3);

  /* Convert the image to HSV. */
  cvCvtColor(image, hsv_image, CV_BGR2HSV);

  /* Keep only red pixels. */
  for (int i = 0; i < hsv_image->width * hsv_image->height; i++) {
    int hue = hsv_image->imageData[3 * i];
    if (hue > 10 && hue < 170) {
      /* Remove non-red pixels. */
      hsv_image->imageData[3 * i + 0] = 0;
      hsv_image->imageData[3 * i + 1] = 0;
      hsv_image->imageData[3 * i + 2] = 0;
    }
  }

  /* Create the grayscale image. */
  IplImage *gray_image = cvCreateImage(cvGetSize(hsv_image), IPL_DEPTH_8U, 1);

  /* Convert the image to grayscale. */
  cvCvtColor(hsv_image, gray_image, CV_BGR2GRAY);

  /* Create the black and white image. */
  IplImage *bw_image = cvCreateImage(cvGetSize(gray_image), IPL_DEPTH_8U, 1);

  /* Threshold the image. */
  cvThreshold(gray_image, bw_image, 50, 255, CV_THRESH_BINARY);

  /* Create the eroded image. */
  IplImage *eroded = cvCreateImage(cvGetSize(bw_image), IPL_DEPTH_8U, 1);

  /* Erode the image. */
  cvErode(bw_image, eroded, NULL, 1);

  /* Create the dilated image. */
  IplImage *dilated = cvCreateImage(cvGetSize(eroded), IPL_DEPTH_8U, 1);

  /* Dilate the image. */
  cvDilate(eroded, dilated, NULL, 1);

  /* Create the image for storing edges. */
  IplImage *edges = cvCreateImage(cvGetSize(dilated), IPL_DEPTH_8U, 1);

  /* Find edges. */
  cvCanny(dilated, edges, 50, 200, 3);

  /* Create a memory storage for storing lines. */
  CvMemStorage* storage = cvCreateMemStorage(0);

  /* Detect lines. */
  CvSeq *lines = cvHoughLines2(edges, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI / 180, 80, 30, 10);

  /* Print the result on stdout. */
  printf("lines count = %d\n", lines->total);

  /* Display the result. */
  if (display) {
    /* Create a temporary grayscale. */
    IplImage *tmp_image = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 1);

    /* Convert the source to grayscale. */
    cvCvtColor(image, tmp_image, CV_BGR2GRAY);

    /* Create the image to display. */
    IplImage *draw_image = cvCreateImage(cvGetSize(tmp_image), IPL_DEPTH_8U, 3);

    /* Add color channels to the grayscale image. */
    cvCvtColor(tmp_image, draw_image, CV_GRAY2BGR);

    /* Draw lines. */
    for (int i = 0; i < lines->total; i++) {
      CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i);
      cvLine(draw_image, line[0], line[1], CV_RGB(255, 0, 0), 1, 8);
    }

    cvShowImage("calibration_bw", dilated);
    cvShowImage("calibration", draw_image);
    while (cvWaitKey(0) != 'q');
  }

  /* We are done using the webcam. */
  cvReleaseCapture(&capture);

  return 0;
}
