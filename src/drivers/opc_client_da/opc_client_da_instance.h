/*
 *                         IndigoSCADA
 *
 *   This software and documentation are Copyright 2002 to 2009 Enscada 
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $HOME/LICENSE 
 *
 *   for full copyright notice and license terms. 
 *
 */

#ifndef OPC_CLIENT_DA__INSTANCE
#define OPC_CLIENT_DA__INSTANCE

#include "opc_client_da.h"
#include "IndentedTrace.h"
#include "fifo.h"
#include "fifoc.h"

//Record format of configuration database
struct iec_item
{
	char opc_server_item_id[99]; //Item ID of opc server, i.e. Simulated Card.Simulated Node.Random.R8
    u_char iec_type;
	DWORD hClient; //Index inside table che � UGUALE a hClient della tabella struct structItem<--------------INDEX, starting from 1
	unsigned char cause; //Per distingure tra spontanee e general interrogation
	u_int   msg_id; //ID of the message
	u_int	checksum; //Checksum of the message
	union {
		double	commandValue;  //rimuovere se non serve il double, quindi anche la union
		char command_string[13];
	};
};


#define	C_SC_NA_1	45
#define	C_EX_IT_1 	200		//custom type
#define	C_IC_NA_1	100     // general interrogation


class Opc_client_da_DriverThread;

class OPC_CLIENT_DADRV Opc_client_da_Instance : public DriverInstance 
{
	Q_OBJECT
	//
	//
	enum
	{
		tUnitProperties = 1,tList, tSamplePointProperties, tListUnits
	};
	//
	//
	
//	QStringList SampleList; // list of sample points
	bool fFail;
	QTimer *pTimer; // timer object for driving state machine
	int Retry; // the retry count
	int Countdown; // the countdown track
	int State; // the state machine's state 
	
	//  
	int Sp; //Current sample point index under measurement
	bool InTick; //tick sentinal
	int OpcItems;
	
	struct  Track
	{
		QString Name;           // name of sample point
		SampleStatistic Stats;  // we track the stats  
		double LastValue;       // the last value
		bool   fSpotValue;        // do we report the last value or the mean of the values over the sample period
		unsigned  SamplePeriod; // how often we sample 
		QDateTime NextSample;
		bool fFailed; // flag if the sample point is in a failed state
		void clear()
		{
			LastValue = 0.0; fSpotValue = false;
			NextSample = QDateTime::currentDateTime();
			fFailed = false;
			Stats.reset();
		}; 
	};
	//
	Track* Values;

	enum // states for the state machine
	{
		STATE_IDLE = 0,
		STATE_READ,
		STATE_WRITE,
		STATE_RESET,
		STATE_FAIL,
		STATE_DONE
	};

	public:
	Opc_client_da_DriverThread *pConnect;
	fifo_h fifo_control_direction;
	unsigned int msg_sent_in_control_direction;

	//
	Opc_client_da_Instance(Driver *parent, const QString &name) : 
	DriverInstance(parent,name),fFail(0), Countdown(1), pConnect(NULL),
	State(STATE_RESET),InTick(0),Retry(0),Sp(0),OpcItems(1), Values(NULL),
	ParentDriver(parent),msg_sent_in_control_direction(0)
	{
		IT_IT("Opc_client_da_Instance::Opc_client_da_Instance");
		connect (GetConfigureDb (),
		SIGNAL (TransactionDone (QObject *, const QString &, int, QObject*)), this,
		SLOT (QueryResponse (QObject *, const QString &, int, QObject*)));	// connect to the database

		pTimer = new QTimer(this);
		connect(pTimer,SIGNAL(timeout()),this,SLOT(Tick()));
		pTimer->start(1000); // start with a 1 second timer

		/////////////////////////////////////////////////////////////////////////////
		//const size_t max_fifo_queue_size = 4*1024;
		const size_t max_fifo_queue_size = 4*65536;
		//Init thread shared fifos
		fifo_control_direction = fifo_open("fifo_opc_command", max_fifo_queue_size);
	};

	~Opc_client_da_Instance()
	{    
		IT_IT("Opc_client_da_Instance::~Opc_client_da_Instance");

		if(Values)
		{
			delete[] Values;
			Values = NULL;
		}
	};
	//
	void Fail(const QString &s)
	{
		FailUnit(s);
		fFail = true;
	};

	InstanceCfg Cfg; // the cacheable stuff
	Driver* ParentDriver;
	QString unit_name;
	
	void driverEvent(DriverEvent *); // message from thread to parent
	bool event(QEvent *e);
	bool Connect();					//connect to the DriverThread
	bool Disconnect();              //disconnect from the DriverThread
	bool DoExec(SendRecePacket *t);
	bool expect(unsigned int cmd);
	void removeTransaction();


	//
	public slots:
	//
	virtual void Start(); // start everything under this driver's control
	virtual void Stop(); // stop everything under this driver's control
	virtual void Command(const QString & name, BYTE cmd, LPVOID lpPa, DWORD pa_length, DWORD ipindex); // process a command for a named unit 
	virtual void QueryResponse (QObject *, const QString &, int, QObject*); // handles database responses
	virtual void Tick();
	//
};

#endif