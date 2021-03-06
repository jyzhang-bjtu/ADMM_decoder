// Copyright 2012 Xishuo Liu, Stark C. Draper, Benjamin Recht
// This program is distributed under the terms of the GNU General Public License.
//
//    The code is part of ADMM Decoder. Modified for MATLAB
//
//    ADMM Decoder is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    ADMM Decoder is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with ADMM Decoder.  If not, see <http://www.gnu.org/licenses/>.
// =========================================================
// MATLAB mex for projecting onto the parity polytope.
// Code written by Xishuo Liu, xliu94 AT wisc DOT edu
// Algorithm details see paper at http://arxiv.org/abs/1204.0556
// Several modifications was made from the original description in the paper. 
//
// How to use this code:
//   1. Compile mex function in Matlab:
//      (1)  put the ProjPolytope.cpp in your MATLAB folder, in MATLAB command line, 
//      (2)  type command "mex - setup" 
//      (3)  choose your compiler (e.g. Visual Studio etc.)
//      (4)  type command "mex ProjPolytope.cpp"
//      (5)  alternatively, you can use g++ compiler and switch on "O3" by "mex -v CC=g++ LD=g++ COPTIMFLAGS='-O3' ProjPolytope.cpp"
//   2. Use the function:
//           
//                      x = ProjPolytope(v)
//           v -- a column vector (N-by-1) which is the vector you want to project
//           x -- results of the projection   
//   3. Examples:
//      (1)   x = ProjPolytope(2*rand(10,1));

#include "mex.h"
#include <cmath>
#include <iostream>
#include <algorithm>

using namespace std;
struct NODE 
{
	int index;
	double value;
}node;
// comparer for nodes. needed for qsort


bool sort_compare_de (const NODE& a, const NODE& b)
{
	return (a.value > b.value);
}

// Total projection, need to search on alpha
double* FastProjection(NODE v[], int length)
{
	double zero_tol_offset = 1e-10;
	double *results = new double[length];
	bool AllZero = true, AllOne = true;
	for(int i = 0; i < length; i++)
	{
		if(v[i].value > 0)
			AllZero = false;
		if(v[i].value <= 1)
			AllOne = false;
	}
	if(AllZero) // exit if the vector has all negative values, which means that the projection is all zero vector
	{
		for(int i = 0;i < length; i++)
			results[i] = 0;
		return results;
	}
	if (AllOne && (length%2 == 0)) // exit if the vector should be all one vector.
	{
		for(int i = 0;i < length; i++)
			results[i] = 1;
		return results;
	}

	NODE *zSort = new NODE[length]; // vector that save the sorted input vector
	for(int i = 0;i < length; i++) // keep the original indices while sorting, 
	{
		zSort[i].index = v[i].index;
		zSort[i].value = v[i].value;
	}
	sort(zSort, zSort + length, sort_compare_de);//sort input vector, decreasing order

	
	int clip_idx = 0, zero_idx= 0;
	double constituent = 0;
	NODE *zClip = new NODE[length]; 
	for(int i = 0;i < length; i++)// project on the [0,1]^d cube
	{
		zClip[i].value = min(max(zSort[i].value, 0.0),1.0);
		zClip[i].index = zSort[i].index;
		constituent += zClip[i].value;
	} 
	int r = (int)floor(constituent);
	if (r & 1)
		r--;
	// calculate constituent parity

	// calculate sum_Clip = $f_r^T z$
	double sum_Clip = 0;
	for(int i = 0; i < r+1; i++)
		sum_Clip += zClip[i].value;
	for(int i = r + 1; i < length; i++)
		sum_Clip -= zClip[i].value;
	

	if (sum_Clip <= r) // then done, return projection, beta = 0
	{
		for(int i = 0; i < length; i++)
			results[zClip[i].index] = zClip[i].value;
		delete [] zSort; delete [] zClip;
		return results;
	}

	double beta = 0;
	double beta_max = 0;
	if (r + 2 <= length)
		beta_max = (zSort[r].value - zSort[r+1].value)/2; // assign beta_max
	else
		beta_max = zSort[r].value;

	NODE *zBetaRep = new NODE[length]; 

	// merge beta, save sort
	int left_idx = r, right_idx = r + 1;
	int count_idx = 0;
	while(count_idx < length)
	{
		double temp_a, temp_b;
		if(left_idx < 0)
		{
			while(count_idx < length)
			{
				zBetaRep[count_idx].index = right_idx;
				zBetaRep[count_idx].value = -zSort[right_idx].value;
				right_idx++;count_idx++;
			}
			break;
		}
		if(right_idx >= length)
		{
			while(count_idx < length)
			{
				zBetaRep[count_idx].index = left_idx;
				zBetaRep[count_idx].value = zSort[left_idx].value - 1;
				left_idx--;count_idx++;
			}
			break;
		}
		temp_a = zSort[left_idx].value - 1; temp_b = -zSort[right_idx].value;
		if(temp_a > temp_b || left_idx < 0)
		{
			zBetaRep[count_idx].index = right_idx;
			zBetaRep[count_idx].value = temp_b;
			right_idx++;count_idx++;
		}
		else
		{
			zBetaRep[count_idx].index = left_idx;
			zBetaRep[count_idx].value = temp_a;
			left_idx--;count_idx++;
		}
	}
	
    //initialize "waterfilling"
	for(int i = 0; i < length; i++)
	{
		if (zSort[i].value > 1)
			clip_idx++;
		if (zSort[i].value >= 0 - zero_tol_offset)
			zero_idx++;
	}
	clip_idx--;
	bool first_positive = true, below_beta_max = true;
	int idx_start = 0, idx_end = 0;
	for(int i = 0;i < length; i++)
	{
		if(zBetaRep[i].value < 0 + zero_tol_offset && first_positive)
		{
			idx_start++;
		}
		if(zBetaRep[i].value < beta_max && below_beta_max)
		{
			idx_end++;
		}
	}
	idx_end--;
	double active_sum = 0;
	for(int i = 0;i < length; i++)
	{
		if(i > clip_idx && i <= r)
			active_sum += zSort[i].value;
		if(i > r && i < zero_idx)
			active_sum -= zSort[i].value;
	}
	double total_sum = 0;
	total_sum = active_sum + clip_idx + 1;

    
	int previous_clip_idx, previous_zero_idx;
	double previous_active_sum;
	double beta_ppp;
	bool change_pre = true;
	previous_clip_idx = clip_idx;
	previous_zero_idx = zero_idx;
	previous_active_sum = active_sum;
	for(int i = idx_start; i <= idx_end; i++)
	{
		if(change_pre)
		{
			// save previous things
			previous_clip_idx = clip_idx;
			previous_zero_idx = zero_idx;
			previous_active_sum = active_sum;
		}
		change_pre = false;

		beta = zBetaRep[i].value;
		if(zBetaRep[i].index <= r)
		{
			clip_idx--;
			active_sum += zSort[zBetaRep[i].index].value;
		}
		else
		{
			zero_idx++;
			active_sum -= zSort[zBetaRep[i].index].value;
		}
		if (i < length - 1)
		{
			if (beta != zBetaRep[i + 1].value)
			{
				total_sum = (clip_idx+1) + active_sum - beta * (zero_idx - clip_idx - 1);
				change_pre = true;
				if(total_sum < r)
					break;
			}
			else
			{// actually you shouldn't go here. just continue
				if(0)
					cout<<"here here"<<endl;
			}
		}
		else if (i == length - 1)
		{
			total_sum = (clip_idx + 1)  + active_sum - beta * (zero_idx - clip_idx - 1);
			change_pre = true;
		}
	}
	if (total_sum > r)
	{
		beta = -(r - clip_idx - 1 - active_sum)/(zero_idx - clip_idx - 1); // set beta for the current beta break point
	}
	else
	{
		beta = -(r - previous_clip_idx - 1 - previous_active_sum)/(previous_zero_idx - previous_clip_idx - 1);// set beta for the previous break point
	}
	for(int i = 0;i < length; i++) // assign values for projection results using beta
	{
		if (i <= r)
		{
			results[zSort[i].index] = min(max(zSort[i].value - beta, 0.0),1.0);
		}
		else
		{
			results[zSort[i].index] = min(max(zSort[i].value + beta, 0.0),1.0);
		}

	}
	delete [] zSort; delete [] zClip; delete [] zBetaRep;
	return results;
}
void ProjPolytope(double *input, double *output, int m, int n)
{
	int length = m;
	NODE *v = new NODE[length];
	for (int i =0; i < length; i++)
	{
		v[i].index = i;
		v[i].value = input[i];
	}
	double *results = FastProjection(v, length);
    for (int i =0; i < length; i++)
	{
        //printf ("%4.2f\n",results[i]);
        output[i] = results[i];
	}
	delete [] results;
}

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, mxArray *prhs[]){
	mxArray *matrixIn;
	matrixIn = prhs[0];
	int m = (int)mxGetM(matrixIn);
	int n = (int)mxGetN(matrixIn);
	plhs[0] = mxCreateDoubleMatrix(m,n,mxREAL);
    double *in;
    double *out;
    in = mxGetPr(prhs[0]);
    out = mxGetPr(plhs[0]);
	ProjPolytope(in, out, m, n);
}
