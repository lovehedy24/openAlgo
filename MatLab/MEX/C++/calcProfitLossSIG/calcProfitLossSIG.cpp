// calcProfitLossCPP.cpp : Defines the entry point for the console application.
// http://www.kobashicomputing.com/node/177 for a reference to x64 bit
//
//
// nlhs Number of output variables nargout 
// plhs Array of mxArray pointers to the output variables varargout
// nrhs Number of input variables nargin
// prhs Array of mxArray pointers to the input variables varargin
//
// Matlab function:
// [cash,openEQ,netLiq,returns] = calcProfitLossMEX(data,sig,bigPoint,cost)
// 
// Inputs:
//		data		A 2-D array of prices in the form of Open | Close
//		sig			An array the same length as data, which gives the quantity bought or sold on a given bar.  Consider Matlab remEchosMEX
//		bigPoint	Double representing the full tick dollar value of the contract being P&L'd
//		cost		Double representing the per contract commission
//
// Outputs:
//		cash		A 2D array of cash debts and credits
//		openEQ		A 2D array of bar to bar openEQ values if there is an open position
//		netLiq		A 2D array of aggregated cash transactions plus the current openEQ if any up to a given observation
//		returns		A 2D array of bar to bar returns
//
// Author: Mark Tompkins
//

#include "stdafx.h"
#include "mex.h"
#include <stdio.h>
#include <deque>

using namespace std;

// Create a struct for convenience
typedef struct tradeEntry
{
	int index;
	int quantity;
	double price;
} tradeEntry;

// Prototypes
int isMember(int arr[], int elements, int search);
int sumQty(const deque<tradeEntry>& x);
tradeEntry createLineEntry(int ID, int qty, double price);

// Macros
	#define isReal2DfullDouble(P) (!mxIsComplex(P) && mxGetNumberOfDimensions(P) == 2 && !mxIsSparse(P) && mxIsDouble(P))
	#define isRealScalar(P) (isReal2DfullDouble(P) && mxGetNumberOfElements(P) == 1)

void mexFunction(int nlhs, mxArray *plhs[], /* Output variables */
int nrhs, const mxArray *prhs[]) /* Input variables */
{
// There are a number of provided functions for interfacing back to Matlab
// mexFuncion		The gateway to C.  Required in every C & C++ solution to allow Matlab to call it
// mexEvalString	Execute Matlab command
// mexCallMatlab	Call Matlab function (.m or .dll) or script
// mexPrintf		Print to the Matlab command window
// mexErrMsgTxt		Issue error message and exit returning control to Matlab
// mexWarnMsgTxt	Issue warning message
// mexPrintf("Hello, world!"); /* Do something interesting */

	

	// Check number of inputs
	if (nrhs != 4)
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:NumInputs",
		"Number of input arguments is not correct. Aborting.");
	
	if (nlhs != 4)
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:NumOutputs",
		"Number of output assignments is not correct. Aborting.");

	// Define constants (#define assigns a variable as either a constant or a macro)
	// Inputs
	#define data_IN		prhs[0]
	#define sig_IN		prhs[1]
	#define bigPoint_IN	prhs[2]
	#define cost_IN		prhs[3]
	// Outputs
	#define cash_OUT	plhs[0]
	#define openEQ_OUT	plhs[1]
	#define netLiq_OUT	plhs[2]
	#define returns_OUT	plhs[3]

	// Init variables
	mwSize rowsData, colsData, rowsSig, colsSig;
    double *cash, *openEQ, *netLiq, *returns, *dataPtr, *sigPtr, *bigPointPtr, *costPtr;

	// Check type of supplied inputs
	if (!isReal2DfullDouble(data_IN)) 
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:BadInputType",
		"Input 'data' must be a 2 dimensional full double array. Aborting.");

	if (!isReal2DfullDouble(sig_IN)) 
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:BadInputType",
		"Input 'sig' must be a 2 dimensional full double array. Aborting.");

	if (!isRealScalar(bigPoint_IN)) 
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:BadInputType",
		"Input 'bigPoint' must be a single scalar double. Aborting.");

	if (!isRealScalar(cost_IN)) 
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:BadInputType",
		"Input 'cost' must be a single scalar double. Aborting.");

	// Assign variables
	rowsData = mxGetM(data_IN);
	colsData = mxGetN(data_IN);
	rowsSig = mxGetM(sig_IN);
	colsSig = mxGetN(sig_IN);
	
	if (rowsData != rowsSig)
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:ArrayMismatch",
		"The number of rows in the data array and the signal array are different. Aborting.");

	if (colsSig > 1)
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:ArrayMismatch",
		"Input 'sig' must be a single column array. Aborting.");

	if (!isRealScalar(bigPoint_IN)) 
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:ScalarMismatch",
		"Input 'bigPoint' must be a double scalar value. Aborting.");

	if (!isRealScalar(cost_IN))  
		mexErrMsgIdAndTxt( "MATLAB:calcProfitLossCPP:ScalarMismatch",
		"Input 'cost' must be a double scalar value. Aborting.");

	/* Create matrices for the return arguments */ 
	// http://www.mathworks.com/help/matlab/matlab_external/c-c-source-mex-files.html
	cash_OUT = mxCreateDoubleMatrix(rowsData, 1, mxREAL);
	openEQ_OUT = mxCreateDoubleMatrix(rowsData, 1, mxREAL); 
	netLiq_OUT = mxCreateDoubleMatrix(rowsData, 1, mxREAL); 
	returns_OUT = mxCreateDoubleMatrix(rowsData, 1, mxREAL); 
	

	/* Assign pointers to the arrays */ 
	dataPtr = mxGetPr(prhs[0]);
	sigPtr = mxGetPr(prhs[1]);
	bigPointPtr = mxGetPr(prhs[2]);
	costPtr = mxGetPr(prhs[3]);

	// assign values to the two variables passed as arrays
	const double bigPoint = bigPointPtr[0];
	const double cost = costPtr[0];

	// assign the variables for manipulating the arrays (by pointer reference)
	cash = mxGetPr(cash_OUT);
	openEQ = mxGetPr(openEQ_OUT);
	netLiq = mxGetPr(netLiq_OUT);
	returns = mxGetPr(returns_OUT);

	// START //
	int	i, numTrades;

	// TRADES ARRAY
	double *trades = new double[rowsSig + 1];

	// Shift signal down one observation
	trades[0] = 0;
	
	// Initialize numTrades counter
	numTrades = 0;
	for (i=1; i<rowsSig+1; i++)					// Remember C++ starts counting at '0'
	{
		trades[i]=*sigPtr++;
		if(trades[i]!=0)
			numTrades++;						// We can either over allocate memory to idxTrades array or do a 2nd iteration
	}											// Currently we'll do a 2nd pass after we know the size of the array
	
	// Check if any trades.  If so, start the P&L process
	if (numTrades > 0)							// We have trades
	{
		int *idxTrades = new int[numTrades];
		int c = 0;
		for (i=0; i<rowsSig+1; i++)
		{
			if (trades[i]!=0)
			{
				idxTrades[c]=i;
				c++;
			}
		}
		
		// Initialize a ledger for open positions
		deque<tradeEntry> openLedger;
		
		// Put first trade on ledger
		openLedger.push_back(createLineEntry(idxTrades[0],trades[idxTrades[0]],dataPtr[idxTrades[0]]));

		int netPO = 0, newPO = 0, needQty = 0;
		for (i = idxTrades[0]; i<rowsSig; i++)
		{
			// Check if we have any additional trades
			// We have a trade. 'i~=idxTrades(1)' so we skip the first entry
			if (i != idxTrades[0] && isMember(idxTrades,numTrades,i))
			{
				// Get net open position netPO
				netPO = sumQty(openLedger);

				if (netPO < 0 && trades[i] < 0 || netPO > 0 && trades[i] > 0)
				{
					// Trade is additive. Add to existing position --> openLedger
					openLedger.push_back(createLineEntry(i,trades[i],dataPtr[i]));
				}
				
				// Trade is an offsetting position
				else	// Offset existing position
				{
					// Check if new trade is larger than existing position
					if (abs(trades[i]) >= abs(netPO))
					{
						// New trade is larger than or equal to existing position. Calculate cash on all ledger lines
						while (openLedger.size() !=0)
						{
							cash[i] = cash[i] + ((dataPtr[i] - openLedger.at(0).price) * openLedger.at(0).quantity * bigPoint) - (abs(openLedger.at(0).quantity) * cost);
							openLedger.pop_front();
						}
						// Reduce new trades by previous position and create new entry if applicable
						newPO = trades[i] + netPO;		// variable to calculate new position size
						if (newPO != 0)
						{
							openLedger.push_back(createLineEntry(i,newPO,dataPtr[i]));

						}
					}
					else
					{
						// New trade is smaller than the current open position.
						// How many do we need to reduce by?
						needQty = trades[i];
						// Prepare to iterate until we are satisfied
						while (needQty !=0)
						{
							// Is the current line item quantity larger than what we need?
							if (abs(openLedger.at(0).quantity) > needQty)
							{
								// If so we will P&L the quantity we need and reduce the open position size
								cash[i] = cash[i] + ((dataPtr[i] - openLedger.at(0).price) * -needQty * bigPoint) - (abs(needQty)*cost);
								// Reduce the position size.  We are aggregating so we add (e.g. 5 Purchases + 4 Sales = 1 Long)
								openLedger.at(0).quantity = openLedger.at(0).quantity + needQty;
								// We are satisfied and don't need any more contracts
								needQty = 0;
							}
							else
							// Current line item quantity is equal to or smaller than what we need.  Process P&L and remove.
							{
								// P&L entire quantity
								cash[i] = cash[i] + ((dataPtr[i] - openLedger.at(0).price) * -openLedger.at(0).quantity * bigPoint) - (abs(openLedger.at(0).quantity)*cost);
								// Reduce needed quantity by what we've been provided
								needQty = needQty + openLedger.at(0).quantity;
								// Remove the line item (FIFO)
								openLedger.pop_front();
							}
						}
					}
				}

			}
			// Calculate current openEQ if there are any positions
			// Make sure we have an open position
			if (openLedger.size() != 0)
			{
				// We will aggregate all line items
				for (int j = 0; j<openLedger.size();j++)
				{
					openEQ[i] = openEQ[i] + ((dataPtr[i+rowsData] - openLedger.at(j).price) * openLedger.at(j).quantity * bigPoint);
				}
			}
		}
		// destroy the deque
		openLedger.~deque();
	}
	
	

	// These are for convenience and could be removed for optimization
	// Calculate a cumulative sum of closed trades and open equity per observation
	double runSum = 0;
	for (int i=0; i<rowsData; i++)
	{
		runSum = runSum + cash[i];
		netLiq[i] = runSum + openEQ[i];
	}

	// Calculate a return from day to day based on the change in value oberservation to observation
	returns[0] = 0;
	for (int i=1; i<rowsData; i++)
	{
		returns[i] = netLiq[i] - netLiq[i-1];
	}

	// BE SURE TO destroy temporary arrays
	delete [] trades;
	trades = NULL;								// Best practice.  Null array pointer.

	

return;
}

/////////////
//
// METHODS
//
/////////////

// Constructor for ledger line item creation
tradeEntry createLineEntry(int ID, int qty, double price)
{
	tradeEntry lineEntry;
		lineEntry.index = ID;
		lineEntry.quantity = qty;
		lineEntry.price = price;

	return lineEntry;
}

// Method to recursively search array for membership. This function works properly.  DO NOT EDIT.
int isMember(int arr[], int elements, int search)  
{  
	if (elements < 0)  
	{
		return 0;  
	}
	if (arr[elements] == search)  
	{
		return 1; 
	}
	else 
	{
		{
		return isMember(arr, elements - 1, search);
		}
	}
} 

// Method to sum the quantity values in any struct of type tradeEntry
int sumQty(const deque<tradeEntry>& x)
{
	int sumOfQty = 0;  // the sum is accumulated here
	// for (int i=0; i<x.size(); i++)
	for (deque<tradeEntry>::const_iterator it=x.begin();it!=x.end();it++)
	{
		//sumOfQty += x[i].price;
		sumOfQty += it->quantity;
	}
	return sumOfQty;
}