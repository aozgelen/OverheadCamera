/*! 
 * \file   OHCamera.cpp
 * \author: Pablo Munoz
 * I am using Robot.cpp as a model. Thanks Mark for the contribution.
 * Created on May 12, 2011, 10:22 AM
 */

#include "definitions.h"
#include "OHCamera.h"
#include <iostream>
#include <string>
#include <stdlib.h>

using namespace metrobotics;

//OHCamera::OHCamera(string host, string dbname, string user, string pwd):mIOService(), mSocket(mIOService), host(host), dbname(dbname), user(user), pwd(pwd){
OHCamera::OHCamera(int cameraNum):mIOService(), mSocket(mIOService), endProgram(false){
  mNameID = UID_OHCAMERA;
  mTypeID = SID_OHCAMERA;
  uniqueRobotIdTracking = -1;
  ident_sent = false;
  camera = cameraNum;
  sendCamposeApproved = false;
  
  skipFrame = 0;
  robotArea = 600;
  beatCircle = 15;
  beatIncrease = true;
  
  // Pablo: I am using this for testing. Please fix to read from Conf file
  realGridUpLeftX = 124;
  realGridUpLeftY = 50;
  realGridUpRightX = 428;
  realGridUpRightY = 50;
  realGridDownLeftX = 120;
  realGridDownLeftY = 287;
  realGridDownRightX = 449; 
  realGridDownRightY = 274;
  tempWidth = 313;
  tempHeigth = 230;
  
  binaryThresholdMin = 220;
  binaryThresholdMax = 250;
  upperLeftCornerX = 50;
  upperLeftCornerY = 50;
  upperRightCornerX = 500;
  upperRightCornerY = 50;
  lowerLeftCornerX = 50;
  lowerLeftCornerY = 500;
  lowerRightCornerX = 500;
  lowerRightCornerY = 500;
  
  capture = 0;
  capture = cvCaptureFromCAM(camera);
  if(!capture)
    {
      cout<<"Could not initialize capturing...\n";
      exit(1);
    }
  
  cvNamedWindow("video", 1);
  
  drop = 0;
  
  posX = -1;
  posY = -1;
  thetaR = -1;
  
  
}

OHCamera::~OHCamera() {
	cvReleaseImage(&frame);
    cvDestroyWindow("video");
	//cvDestroyWindow("processed");
	capture = 0;
    cvReleaseCapture(&capture);    
    disconnect();
}
void OHCamera::init_state()
{
	mCurrentState = STATE_INIT;
	mPreviousState = -1;
	mSessionID    = -1;
}
void OHCamera::update(){
	imageLoop(); // TODO: Put this in a different thread.
    // Prepend function signature to error messages.
	static const string signature = "OHCamera::Update()";
	
	// Save us some typing -- I'm already feeling the effects of RSI.
	using namespace boost::system;
	using namespace boost::asio;
	
	// Make sure that the socket is already open.
	if (!mSocket.is_open()) return;
	
	error_code ec; // Used to check for errors.
	
	// Maintain the state machine.
	if(mPreviousState != mCurrentState){
		cout << "Current state: " << mCurrentState << endl;
	}
	mPreviousState = mCurrentState;
	switch (mCurrentState) {
		case STATE_INIT: {
			do_state_action_init();
		} break;
		case STATE_ACK: {
			do_state_action_ack();
		} break;
		case STATE_IDLE: {
			do_state_action_idle();
		} break;
		case STATE_CAMPOSE_SEND: {
			do_state_action_campose_send();
		} break;
		case STATE_IDENT: {
			do_state_action_ident();
		} break;
		case STATE_PING_SEND: {
			do_state_action_ping_send();
		} break;
		case STATE_PONG_READ: {
			do_state_action_pong_read();
		} break;
			//		case STATE_PONG_SEND: {
			//			do_state_action_pong_send();
			//		} break;
		case STATE_CMD_PROC: {
			do_state_action_cmd_proc();
		} break;
			//		case STATE_POSE: {
			//			do_state_action_pose();
			//		} break;
		default: {
			cerr << signature << " - unrecognized state" << endl;
			do_state_change(STATE_QUIT);
		} break;
	}
	
}
bool OHCamera::connect(const string& hostname, unsigned short port){
	using namespace boost::system;
	using namespace boost::asio;
	
	
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::Connect()";
	
	// Make sure that the socket isn't already open.
	if (mSocket.is_open()) {
		disconnect();
	}
	
	error_code ec; // Used to check for errors.
	
	// Get the IP address of the host.
	ip::address addr = ip::address::from_string(hostname, ec);
	if (ec) {
		std::cerr << signature << " - failed to retrieve host's IP address" << std::endl;
		return false;
	}
	
	// Connect to the host.
	ip::tcp::endpoint endpt(addr, port);
	mSocket.connect(endpt, ec);
	if (ec) {
		std::cerr << signature << " - failed to connect to host" << std::endl;
		return false;
	}
	
	// Go back to the initial state.
	init_state();  
	return true;
}

void OHCamera::disconnect(){
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::Disconnect()";
	
	using namespace boost::system;
	using namespace boost::asio;
	
	// Make sure that the socket is already open.
	if (!mSocket.is_open()) return;
	
	error_code ec; // Used to check for errors.
	
	// Shutdown the connection.
	mSocket.shutdown(ip::tcp::socket::shutdown_both, ec);
	if (ec) {
		std::cerr << signature << " - failed to disconnect" << std::endl;
	}
	
	// Close the socket.
	mSocket.close(ec);
	if (ec) {
		std::cerr << signature << " - failed to close the socket" << std::endl;
	}
}
void OHCamera::do_state_action_init()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_init()";
	cout << "Sending INIT" << endl;
	// Send the INIT command.
	stringstream ss;
	ss << CMD_INIT << " " << mTypeID << " " << mNameID << " 0 0";
	if (write(ss)) {
		cerr << signature << " - success; next state: STATE_ACK" << endl;
		do_state_change(STATE_ACK);
	} else if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_QUIT" << endl;
		do_state_change(STATE_QUIT);
	} else {
		cerr << signature << " - failure; next state: STATE_INIT" << endl;
		do_state_change(STATE_INIT);
	}
}
void OHCamera::do_state_action_campose_send()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_campose_send()";
	stringstream ss;
	ss << CMD_CAMPOSE << " " << uniqueRobotIdTracking << " " << posX << " " << posY << " " << thetaR;
	sleep(1);
	if (write(ss)) {
		cerr << signature << " - success; next state: STATE_IDLE" << endl;
		mStateTimer.start();
		do_state_change(STATE_IDLE);
	} else if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_QUIT" << endl;
		do_state_change(STATE_QUIT);
	} else {
		cerr << signature << " - failure; next state: STATE_IDLE" << endl;
		do_state_change(STATE_IDLE);
	}
	
}
void OHCamera::do_state_action_ident()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_ident()";
	stringstream ss;
	ss << CMD_IDENT;
	if (write(ss)) {
		cerr << signature << " - success; next state: STATE_IDLE" << endl;
		mStateTimer.start();
		do_state_change(STATE_IDLE);
	} else if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_QUIT" << endl;
		do_state_change(STATE_QUIT);
	} else {
		cerr << signature << " - failure; next state: STATE_IDLE" << endl;
		do_state_change(STATE_IDLE);
	}
}
void OHCamera::do_state_change(int state)
{
	// Don't make false changes.
	if (mCurrentState != state) {
		if (state == STATE_INIT) {
			init_state();
		} else {
			mCurrentState = state;
		}
		// Update the state timer.
		mStateTimer.start();
	}
}
void OHCamera::do_state_action_ack()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_ack()";
	
	// Don't wait forever.
	if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_INIT" << endl;
		do_state_change(STATE_INIT);
	}
	
	// Don't block while waiting for the command.
	if (!msg_waiting()) return;
	
	// Prepare to read the command.
	stringstream ss;
	string cmd;
	long session_id;
	if (read(ss) && (ss >> cmd >> session_id) && (cmd.find(CMD_ACK) != string::npos)) {
		if (session_id < 0) {
			mSessionID = -1;
			cerr << signature << " - rejected; next state: STATE_QUIT" << endl;
			do_state_change(STATE_QUIT);
		} else {
			mSessionID = session_id;
			cerr << signature << " - accepted; next state: STATE_IDLE" << endl;
			do_state_change(STATE_IDLE);
		}
	} else {
		cerr << signature << " - failure; next state: STATE_INIT" << endl;
		do_state_change(STATE_INIT);
	}
}
bool OHCamera::msg_waiting() const
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::msg_waiting()";
	
	// Save us some typing -- I'm already feeling the effects of RSI.
	using namespace boost::system;
	using namespace boost::asio;
	
	// Make sure that the socket is already open.
	if (!mSocket.is_open()) return false;
	
	error_code ec; // Used to check for errors.
	
	// Check the socket.
	size_t payload = mSocket.available(ec);
	if (ec) {
		cerr << signature << " - failed to peek at the socket" << endl;
		return false;
	} else if (payload >= sizeof(cmd_len_t)) {
		return true;
	} else {
		return false;
	}
}
void OHCamera::do_state_action_idle()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_idle()";
	
	// Keep an eye out for new commands.
	if (msg_waiting()) {
		cerr << signature << " - received command; next state: STATE_CMD_PROC" << endl;
		do_state_change(STATE_CMD_PROC);
	} else if (mSilenceTimer.elapsed() >= MAX_TIME_SILENCE) {
		// Don't let the connection die.
		cerr << signature << " - max silence exceeded; next state: STATE_PING_SEND" << endl;
		do_state_change(STATE_PING_SEND);
	} else {
		// PABLO TODO: Create a beat for the CAMPOSE
		if(uniqueRobotIdTracking==-1 && ident_sent == false){
			do_state_change(STATE_IDENT);
			ident_sent = true;
			return;
		}
		else if(uniqueRobotIdTracking != -1 && sendCamposeApproved){
			do_state_change(STATE_CAMPOSE_SEND);
			sendCamposeApproved = false;
		}
	}
}

void OHCamera::do_state_action_cmd_proc()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_cmd_proc()";
	
	// Don't block while waiting for the command.
	
	if (!msg_waiting()) return;
		// Prepare to read the command.
	stringstream ss;
	string cmd;
	if (read(ss) && (ss >> cmd)) {
		cout << "if read(ss) " << cmd << " " << ss << endl;
		// Process the command.
		if (cmd.find(CMD_PING) != string::npos){
			cerr << signature << " - PING; next state: STATE_PONG_SEND" << endl;
			do_state_change(STATE_PONG_SEND);
		}
		else if (cmd.find(CMD_DBREGPOS) != string::npos){
			// Pablo: I deleted this code because it was unnecessary here. If needed look in DbManager.
			cerr << signature << " - success; next state: STATE_IDLE" << endl;
			do_state_change(STATE_IDLE);
		}
		else if(cmd.find(CMD_DBREGUSAC)!= string::npos){
			// Pablo: I deleted this code because it was unnecessary here. If needed look in DbManager.
			cerr << signature << " - success; next state: STATE_IDLE" << endl;
			do_state_change(STATE_IDLE);
		} else if(cmd.find(CMD_IDENT)!= string::npos){
			cout << "Ident Received" << endl;
			mStringBuffer = ss.str(); 
			string command; 
			uint32_t num_bots; 
			long robot_id; 
			string robot_name; 
			string robot_type;
			uint32_t num_provides; 
			string provide; 
			stringstream iss(mStringBuffer); 
			cout << "mStringBuffer: " << mStringBuffer << endl;
			if(!(iss >> command >> num_bots)){
				stringstream oss; oss << CMD_ERROR << " " << CMD_IDENT << " failed: invalid arguments"; write(oss);
				cerr << signature << " - failure; next state: STATE_IDLE" << endl; do_state_change(STATE_IDLE); return;
			}
			
			for(int i=0; i<num_bots; i++){
				if(!(iss >> robot_id >> robot_name >> robot_type >> num_provides)){
					stringstream oss; 
					oss << CMD_ERROR << " " << CMD_IDENT << " failed: invalid arguments";
					write(oss); 
					cerr << signature << " - failure; next state: STATE_IDLE" << endl;
					do_state_change(STATE_IDLE); return;
				}
				else {
					for(int j = 0; j<num_provides; j++){
						if(!(iss >> provide)){
							cout << "Reading provide: " << provide << endl;
							stringstream oss; 
							oss << CMD_ERROR << " " << CMD_IDENT << " failed: invalid arguments";
							write(oss); 
							cerr << signature << " - failure; next state: STATE_IDLE" << endl;
							do_state_change(STATE_IDLE); return;
						}
					}
				}

				uniqueRobotIdTracking = robot_id; 
				cout << "Unique Robot ID: " << uniqueRobotIdTracking << endl;
			}
			cout << uniqueRobotIdTracking << endl; 
			do_state_change(STATE_IDLE); return;
		} else if(cmd.find(CMD_QUIT) != string::npos)
		{
			cerr << signature << " - QUIT; next state: STATE_QUIT" << endl;
			do_state_change(STATE_QUIT);
		} else
		{
			cerr << signature << " - unrecognized command; next state: STATE_IDLE" << endl;
			do_state_change(STATE_IDLE);
			return;
		} 
	} else {
		cerr << signature << " - failure; next state: STATE_IDLE" << endl;
		cout << "What??" << endl;
		do_state_change(STATE_IDLE);
	}
}
void OHCamera::do_state_action_pong_read()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_pong_read()";
	
	// Don't wait forever.
	if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_PING_SEND" << endl;
		do_state_change(STATE_PING_SEND);
	}
	
	// Don't block while waiting for the command.
	if (!msg_waiting()) return;
	
	// Prepare to read the command.
	stringstream ss;
	string cmd;
	if (read(ss) && (ss >> cmd) && (cmd.find(CMD_PONG) != string::npos)) {
		cerr << signature << " - success; next state: STATE_IDLE" << endl;
		do_state_change(STATE_IDLE);
	} else {
		cerr << signature << " - failure; next state: STATE_PING_SEND" << endl;
		do_state_change(STATE_PING_SEND);
	}
}
bool OHCamera::read(std::stringstream& ss)
{
	
	
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::read()";
	
	// Make sure that the socket is already open.
	if (!mSocket.is_open()) return false;
	
	// Read the message.
	boost::asio::streambuf inputBuffer;
	try {
		
		cmd_len_t len;
		cout << boost::asio::read(mSocket, boost::asio::buffer(&len, sizeof(cmd_len_t)));
		cout << endl <<  "len: " << len << endl;
		size_t n = boost::asio::read(mSocket, inputBuffer.prepare(len));
		cout << n << endl;
		cout << "here" << endl;
		inputBuffer.commit(n);
		mSilenceTimer.start();
		
	} catch (boost::system::system_error) {
		cerr << signature << " - failed to read message" << endl;
		return false;
	}
	// Convert the message into a workable format.
	istream is(&inputBuffer);
	// Clear the contents of the argument.
	ss.str("");
	// Fill the argument with the message that we just read.
	ss << is.rdbuf();
	
	// Spam standard out so that the user can feel like they're in the Matrix.
	cout << mNameID << "::read = [" << ss.str() << "]" << endl;
	
	return true;
}
bool OHCamera::write(const stringstream& ss)
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::write()";
	
	// Make sure that the socket is already open.
	if (!mSocket.is_open()) return false;
	
	// Compute the maximum size of a message.
	const static size_t num_bits = 8 * sizeof(cmd_len_t);
	const static string::size_type max_size = (1 << num_bits) - 1;
	
	// Make sure that the message doesn't exceed the maximum size.
	string msg = ss.str();
	if (msg.size() > max_size) {
		cerr << signature << " - message is too large" << endl;
		return false;
	}
	
	// Build the message.
	boost::asio::streambuf outputBuffer;
	ostream os(&outputBuffer);
	// First the preamble: size of the message.
	cmd_len_t len = static_cast<cmd_len_t>(msg.size());
	os.write(reinterpret_cast<const char *>(&len), static_cast<streamsize>(sizeof(cmd_len_t)));
	// Now the message itself.
	os << msg;
	
	// Send the message.
	try {
		size_t n = boost::asio::write(mSocket, outputBuffer.data());
		outputBuffer.consume(n);
		mSilenceTimer.start();
	} catch (boost::system::system_error) {
		cerr << signature << " - failed to send message" << endl;
		return false;
	}
	
	
	return true;
}
void OHCamera::do_state_action_ping_send()
{
	// Prepend function signature to error messages.
	static const string signature = "OHCamera::do_state_action_ping_send()";
	
	// Send the PING command.
	stringstream ss;
	ss << CMD_PING;
	if (write(ss)) {
		cerr << signature << " - success; next state: STATE_PONG_READ" << endl;
		do_state_change(STATE_PONG_READ);
	} else if (mStateTimer.elapsed() >= MAX_TIME_STATE) {
		cerr << signature << " - timeout; next state: STATE_INIT" << endl;
		do_state_change(STATE_INIT);
	} else {
		cerr << signature << " - failure; next state: STATE_PING_SEND" << endl;
		do_state_change(STATE_PING_SEND);
	}
}

int OHCamera::imageLoop(){
	frame = cvQueryFrame(capture);
	if(!frame)
		return 0; //break;
	
	drop++;
	if(drop%2==0){
		return 0; //continue;
	}
	//finalFrame = doPyrDown(frame);
	//finalFrame = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 3);
	gray_im = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
	binary_im = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
	cvCvtColor(frame, gray_im, CV_RGB2GRAY);
	cvThreshold(gray_im, binary_im, binaryThresholdMin, binaryThresholdMax, CV_THRESH_BINARY); 
	cvErode(binary_im, binary_im);
	
	findShapes(binary_im, robotArea, frame);
	
	//cout << "skipFrame = " << skipFrame << endl;
	if(skipFrame==1){
		cout << "No Robot" << endl;
		
		//cvReleaseImage(&frame);
		skipFrame =0;
		cvShowImage("video", frame);
		//cvShowImage("processed", binary_im);
		cvReleaseImage(&gray_im);
		cvReleaseImage(&binary_im);
		checkKey();

		return 0; //continue;
	}
	// Draw Grid for more accuracy
	//cvLine(frame, cvPoint(upperLeftCornerX, upperLeftCornerY),cvPoint(upperRightCornerX, upperRightCornerY) , cvScalar(255));
	//cvLine(frame, cvPoint(upperRightCornerX, upperRightCornerY),cvPoint(lowerRightCornerX, lowerRightCornerY) , cvScalar(255));
	//cvLine(frame, cvPoint(lowerRightCornerX, lowerRightCornerY),cvPoint(lowerLeftCornerX, lowerLeftCornerY) , cvScalar(255));
	//cvLine(frame, cvPoint(lowerLeftCornerX, lowerLeftCornerY),cvPoint(upperLeftCornerX, upperLeftCornerY) , cvScalar(255));
	
	// Approve sending the message to Skygrid
	sendCamposeApproved = true;
	
	cvShowImage("video", frame);
	//cvShowImage("processed", binary_im);
	checkKey();

	//binaryThresholdMin += 10;
	cout << binaryThresholdMin << endl;
	//end = false;
	//robotArea+= 5;
	//cout << "Robot Area: " << robotArea << endl;
	//checkKey();
	if(endProgram){
		disconnect();
	}
	
	cvReleaseImage(&gray_im);
	cvReleaseImage(&binary_im);
	//cvReleaseImage(&finalFrame);
		
}


int OHCamera::getLineLength(CvPoint* p0, CvPoint * p1){
	return sqrt(pow((double)(p1->x - p0->x),2) + pow((double)(p1->y - p0->y),2)) ;
}

IplImage* OHCamera::doPyrDown( IplImage* in, int	filter){
	
    // Best to make sure input image is divisible by two. //
    assert( in->width%2 == 0 && in->height%2 == 0 );
    IplImage* out = cvCreateImage( cvSize( in->width/2, in->height/2 ), in->depth, in->nChannels);
    cvPyrDown( in, out ); return( out );
}

void OHCamera::findShapes(IplImage* img, int robotArea, IplImage* ret)
{
    int centerx = 0;
    int centery = 0;
    double deltaX = 0;
    double deltaY = 0;
	double theta = 0;
	
    CvSeq* contours;
    CvSeq* result;
    CvMemStorage *storage = cvCreateMemStorage(0);
    //IplImage* ret = cvCreateImage(cvGetSize(img), 8, 3);
    //IplImage* temp = cvCreateImage(cvGetSize(img), 8, 1);
    //cvCvtColor(img, temp, CV_BGR2GRAY);
    IplImage* temp = cvCloneImage(img);
	
    cvFindContours(temp, storage, &contours, sizeof(CvContour),
				   CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
	skipFrame = 1;
    while(contours)
    {
		
        result = cvApproxPoly(contours, sizeof(CvContour), storage,
							  CV_POLY_APPROX_DP, cvContourPerimeter(contours)*0.02, 1);
        if(result->total==4 && fabs(cvContourArea(result, CV_WHOLE_SEQ)) > robotArea-200 && fabs(cvContourArea(result, CV_WHOLE_SEQ)) < robotArea+200)
        {
			cout << "Robot Area: " << fabs(cvContourArea(result, CV_WHOLE_SEQ)) << endl;
			//cout << "Robot Detected" << endl;
            CvPoint *pt[4];
            for(int i=0;i<4;i++)
                pt[i] = (CvPoint*)cvGetSeqElem(result, i);
			
            cvLine(ret, *pt[0], *pt[1], cvScalar(255));
            cvLine(ret, *pt[1], *pt[2], cvScalar(255));
            cvLine(ret, *pt[2], *pt[3], cvScalar(255));
            cvLine(ret, *pt[3], *pt[0], cvScalar(255));
            //counter++;
            //printf("Point:\n[0]x:%d y:%d\n[1]x:%d y:%d\n[2]x:%d y:%d\n[3]x:%d y:%d\n",
            //        pt[0]->x, pt[0]->y, pt[1]->x, pt[1]->y, pt[2]->x, pt[2]->y, pt[3]->x, pt[3]->y );
			
			//            double centerx1 = (double)(pt[0]->x + pt[2]->x)/2;
			//            double centery1 = (double)(pt[0]->y + pt[2]->y)/2;
			//            double centerx2 = (double)(pt[1]->x + pt[3]->x)/2;
			//            double centery2 = (double)(pt[1]->y + pt[3]->y)/2;
			
            int p0x = pt[0]->x;
            int p1x = pt[1]->x;
            int p2x = pt[2]->x;
            int p3x = pt[3]->x;
            int p0y = pt[0]->y;
            int p1y = pt[1]->y;
            int p2y = pt[2]->y;
            int p3y = pt[3]->y;
			
            int centerx = (p0x + p1x + p2x + p3x)/4;//(centerx1 + centerx2)/2;
            int centery = (p0y + p1y + p2y + p3y)/4;//(centery1 + centery2)/2;
            //printf("Center Color Yellow x: %d y:%d\n", centerx, centery);
            cvCircle(ret, cvPoint(centerx, centery), 5, cvScalar(255));
            //Circle(img, center, radius, color, thickness=1, lineType=8, shift=0)
			cout << "Robot is at: " << centerx << ", " << centery << endl;
			
			// Theta
			if(getLineLength(pt[0], pt[1]) > getLineLength(pt[1], pt[2])){
				// 0 - 1 and 2-3
				cvLine(ret, cvPoint(p0x, p0y), cvPoint(p1x, p1y), cvScalar(50), 3);
				cvLine(ret, cvPoint(p2x, p2y), cvPoint(p3x, p3y), cvScalar(50), 3);
				
				
				deltaX = (double)p1x - p0x;
				if(deltaX == 0){
					deltaX = 0.01;
				}
				deltaY = (double)(p1y - p0y);
				double avgDeltaX = deltaX;
				double avgDeltaY = deltaY;
				
				deltaX = (double)p3x - p2x;
				if(deltaX == 0){
					deltaX = 0.01;
				}
				deltaY = (double)p3y-p2y;
				avgDeltaX += deltaX;
				avgDeltaY += deltaY;
				
				avgDeltaX /= 2;
				avgDeltaY /= 2;
				
				theta = atan2(avgDeltaY, avgDeltaX);
				
			}
			else {
				// 1 - 2 and 3-0
				//cout << "Here" << endl;
				//cout << "p2: " << p2x << ", " << p2y << endl;
				//cout << "p1: " << p1x << ", " << p1y << endl;
				
				cvLine(ret, cvPoint(p1x, p1y), cvPoint(p2x, p2y), cvScalar(50), 3);
				cvLine(ret, cvPoint(p0x, p0y), cvPoint(p3x, p3y), cvScalar(50), 3);
				
				
				deltaX = (double)p2x - p1x;
				if(deltaX == 0){
					deltaX = 0.01;
				}
				deltaY = (double)(p2y - p1y);
				
				
				//double avgDeltaX = deltaX;
				//double avgDeltaY = deltaY;
				
				//deltaX = (double)p3x - p0x;
				//if(deltaX == 0){
				//	deltaX = 0.01;
				//}
				//deltaY = (double)p3y-p0y;
				//avgDeltaX += deltaX;
				//avgDeltaY += deltaY;
				
				//avgDeltaX /= 2;
				//avgDeltaY /= 2;
				
				//theta = atan2(avgDeltaY, avgDeltaX);
				theta = atan2(deltaX, deltaY);
				
				
			}
			theta += M_PI/2; 
			if( (theta > M_PI) )
                theta = theta-(2*M_PI);
			
			//            angle = M_PI - angle;
			//        if((deltaX < 0 && deltaY < 0)||(deltaX > 0 && deltaY > 0)){
			//            angle += M_PI/2;//-1 * (M_PI - angle);
			//        }
			//        if(deltaX < 0 && deltaY >0){
			//            angle += M_PI;
			//        }
			
			
			theta = (theta *180)/M_PI;
			// Show data on Screen
			cvCircle(ret, cvPoint(centerx, centery), beatCircle, cvScalar(255));
			
			centerx = realGridUpLeftX + (tempWidth*centerx)/ret->width;
			centery = realGridUpLeftY + (tempHeigth*centery)/ret->height;
			
			
			cout << "Robot in Grid is at: " << centerx << ", " << centery << ", " << theta << endl;
			
			posX = centerx;
			posY = centery;
			thetaR = theta;
			
            
			//centerx = centerx;
            //centery = ret->height - centery;
			
			
			
			
			
			skipFrame = 0;
        }
        contours = contours->h_next;
    }
	if(beatIncrease){
		beatCircle +=8;
		if(beatCircle > 40)
			beatIncrease = false;
	}
	else {
		beatCircle -=8;
		if(beatCircle < 15)
			beatIncrease = true;
	}
    cvReleaseImage(&temp);
    cvReleaseMemStorage(&storage);
    //return *ret;
	return;
}

void OHCamera::checkKey(){
	int keyPressed = cvWaitKey(30); //10
	switch(keyPressed){
		case'a':
			robotArea++;
			
			break;
		case'z':
			robotArea--;
			if(robotArea<=0){
				robotArea = 1;
			}
			break;
			
			
		case'q':
			endProgram = true;
			break;
	}
}