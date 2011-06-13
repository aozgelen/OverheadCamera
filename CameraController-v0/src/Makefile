CPP           	= g++

LIB_DIR		= ../../../lib 

METROUTIL     	= $(LIB_DIR)/libMetrobotics.a
METROUTIL_DIR	= ../../../utils/MetroUtil
METROUTIL_LIB 	= $(LIB_DIR)
METROUTIL_INC 	= $(METROUTIL_DIR)/include

INC_FLAGS	= -I$(METROUTIL_INC)
CPPFLAGS	= $(INC_FLAGS) `pkg-config --cflags opencv`
CXXFLAGS      	= -c -g #-Wall
LIBS   		= -lboost_system-mt `pkg-config --libs opencv` -L$(METROUTIL_LIB) -lMetrobotics -lboost_thread-mt

.SUFFIXES:
.SUFFIXES: .o .cpp

.cpp.o:
	$(CPP) $(CPPFLAGS) $(CXXFLAGS) $*.cpp 

all: CameraController

CameraController: OHCamera.o main.o 
#	echo $(CPP) $(LIBS) -o $@ $^
	$(CPP) $^ $(LIBS) -o $@ 

clean:
	rm -rf *.o CameraController

main.o: 	main.cpp definitions.h OHCamera.h
OHCamera.o: 	OHCamera.cpp definitions.h 