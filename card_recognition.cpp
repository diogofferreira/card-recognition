/*
 * card_recognition.cpp
 *
 * DATASET GENERATOR
 *
 * Diogo Ferreira - Pedro Martins - 2017
 */


#include <iostream>

#include <math.h>

#include <map>

#include <stdlib.h>

#include <dirent.h>

#include "opencv2/core/core.hpp"

#include "opencv2/imgproc/imgproc.hpp"

#include "opencv2/highgui/highgui.hpp"

#include "opencv2/core/mat.hpp"

using namespace std;
using namespace cv;

void preProcessImage(Mat &originalImage)
{
    // Convert to a single-channel, intensity image
    if (originalImage.channels() > 1)
	    cvtColor(originalImage, originalImage, COLOR_BGR2GRAY, 1);

    // Apply Gaussian Filter to get only the countour of the symbols and
    // remove all the artifacts
    int n = 3;
    double sigmaX = 100.0;
    GaussianBlur(originalImage, originalImage, Size(n, n), sigmaX);

    // Apply threshold
    int thresholdValue = 200;
    int thresholdType = THRESH_BINARY;
    int maxValue = 255;
    threshold( originalImage, originalImage, 
            thresholdValue, maxValue, thresholdType);
    
    // Resize image to allow diff calculations 
    resize(originalImage, originalImage, Size(450, 480));
}

// From: https://stackoverflow.com/questions/13495207/opencv-c-sorting-contours-by-their-contourarea
bool reverseCompareContourArea(vector<Point> c1, 
        vector<Point> c2)
{
    return contourArea(c1, false) > contourArea(c2, false);
}

void findContours(const Mat &image, int numCards,
        vector<vector<Point> > &cardsContours)
{
    vector<vector<Point> > contours;
    //vector<Vec4i> hierarchy;
    Mat cannyOutput;
    int mode = CV_RETR_TREE;
    int method = CV_CHAIN_APPROX_SIMPLE;

    Canny(image, cannyOutput, 120, 240);
    
    findContours(cannyOutput, cardsContours, mode, method, Point(0, 0));

    // Find the most common contours
    sort(contours.begin(), contours.end(), reverseCompareContourArea);
    cardsContours.resize(numCards);
}

void transformCardContours(const Mat &image, vector<Mat> &cards,
        const vector<vector<Point> > &cardsContours)
{
    vector<Point> points;

    // Transform perspective card into a 500x500 image card
    for (int i = 0; i < cardsContours.size(); i++) {
        
        Mat card;

        points = cardsContours[i];
        
        double epsilon = 0.1 * arcLength(points, true);
        vector<Point> approxCurve;
        approxPolyDP(points, approxCurve, epsilon, true);
        RotatedRect boxRect = minAreaRect(approxCurve);
        
        /* 
        // Debug rect
        Mat drawing = Mat::zeros( image.size(), CV_8UC3 );
        Scalar color = Scalar(255, 255, 255);
        // contour
        drawContours( drawing, cardsContours, i, color, 1, 8, vector<Vec4i>(), 0, Point() );
        Point2f rect_points[4]; boxRect.points( rect_points );
        for( int j = 0; j < 4; j++ )
          line( drawing, rect_points[j], rect_points[(j+1)%4], color, 1, 8 );
         
        /// Show in a window
        namedWindow( "Contours", CV_WINDOW_AUTOSIZE );
        imshow( "Contours", drawing ); 
        */

        Point2f pts[4];
        boxRect.points(pts);

        Point2f src_vertices[4];
        
        // Correct order 
        //  2--3
        //  |  |
        //  0--1 
        src_vertices[0] = pts[1];
        src_vertices[1] = pts[2];
        src_vertices[2] = pts[0];
        src_vertices[3] = pts[3];
        
        //src_vertices[0] = pts[1];
        //src_vertices[1] = pts[0];
        //src_vertices[2] = pts[2];
        //src_vertices[3] = pts[3];
        
        Point2f dst_vertices[4];
        dst_vertices[0] = Point2f(0, 0);
        dst_vertices[1] = Point2f(449, 0); 
        dst_vertices[3] = Point2f(449, 449);        
        dst_vertices[2] = Point2f(0, 449);        

        //Mat warpAffineMatrix = getAffineTransform(src_vertices, dst_vertices);
        Mat transform = getPerspectiveTransform(src_vertices, dst_vertices);
        cv::Size size(450, 480);
        //warpAffine(image, card, warpAffineMatrix, size, INTER_LINEAR, BORDER_CONSTANT);
        warpPerspective(image, card, transform, size);
        
        /*
        boxRect.center = Point(image.cols/2, image.rows/2);

        // Get the Rotation Matrix
        Mat M = getRotationMatrix2D(boxRect.center, 180, 1.0);
        // Perform the affine transformation
        warpAffine(image, card, M, image.size(), INTER_CUBIC);
        */
        imwrite("rotated.jpg", card);
        
        cards.push_back(card);
    }
    
}

void getTrainingSet(const string path, 
        map<string, Mat> &cardDataset)
{
    DIR* dirp = opendir(path.c_str());
    struct dirent * dp;

    // Read all images in the training folder
    while ((dp = readdir(dirp)) != NULL) {
        string filename = dp->d_name;
        if (filename != "." and filename != "..") {
            filename = path + filename; 
            
            Mat image = imread(filename);
            
            // Pre-process image and save in dataset 
            preProcessImage(image);
            cardDataset[filename] = image;
        }
    }
}

int countBinaryWhite(Mat card) 
{
    int count = 0;
    for (int i = 0; i < card.rows; i++) {
        for (int j = 0; j < card.cols; j++) {
            if (card.at<uchar>(i, j) == 255)
                count++;
        }
    }
    return count;
}

void getClosestCard(Mat &card, map<string, Mat> &cards, 
        String &cardName)
{
    int i = -1;
    int diff, tmpDiff;
    map<string, Mat>::iterator it = cards.begin();

    while(it != cards.end()) {
        
	    // Display difference
        namedWindow( "Imagem Original"+i, WINDOW_AUTOSIZE );
	    imshow("Imagem Original"+i, abs(card - it->second));
        

        if (!++i){
            diff = countBinaryWhite(abs(card - it->second));
            cardName = it->first;
        } else {
            tmpDiff = countBinaryWhite(abs(card - it->second));
            if (tmpDiff < diff) {
                diff = tmpDiff;
                cardName = it->first;
            }
        }
        it++;
    }
}

int main( int argc, char** argv )
{
    
    if( argc != 2 )
    {
		cout << "The name of the image file is missing !!" << endl;

        return -1;
    }

    
    // Get dataset
    int numCards = 1;
    map<string, Mat> cardset; 
    getTrainingSet("./training_set/", cardset);

    Mat originalCard;

	originalCard = imread( argv[1], IMREAD_UNCHANGED );

	if( originalCard.empty() )
	{
		// NOT SUCCESSFUL : the data attribute is empty
		cout << "Image file could not be open !!" << endl;
	    return -1;
	}

    preProcessImage(originalCard);
    /*
    // Display transformation
    namedWindow("Transformed", WINDOW_AUTOSIZE );
    imshow("Transformed", originalCard);
    */
    vector<vector<Point> > cardsContours;
    findContours(originalCard, numCards, cardsContours);
    vector<Mat> cards;
    transformCardContours(originalCard, cards, cardsContours);
    
    for (int i = 0; i < cards.size(); i++) {
        Mat card = cards[i];
        String closestCard;
        getClosestCard(card, cardset, closestCard);
		cout << "\nClosest card = " + closestCard << endl;
    }

    waitKey(0);
    destroyAllWindows();
    
    /* Read camera */
    
    // open default camera 
    /*VideoCapture cap(0);
    
    if(!cap.isOpened())
        cout << "Could not read camera" << endl;

    namedWindow("Camera", 1);
    
    while(true)
    {
        Mat frame;
        // get a new frame from camera
        cap >> frame;
        imshow("Camera", frame);
        if(waitKey(30) >= 0) break;
    }*/
    
    /*
    // Create window

    namedWindow( "Imagem Original", WINDOW_AUTOSIZE );

	// Display image

	imshow( "Imagem Original", originalImage );

	// Print some image features

	cout << "ORIGINAL IMAGE" << endl;

    printImageFeatures( originalImage );

    
	// Destroy the windows

	destroyAllWindows();
    */

	return 0;
}
