#ifndef MY_MATHS_TOOLS_H
#define MY_MATHS_TOOLS_H

// Add a doxygen group for this module.
// If you have more than one file, this should be in only one of them.
/**
 * \defgroup my_maths_tools Description of the my_maths_tools
 */

#include <deque>
#include <eigen3/Eigen/Eigen>

namespace ns3 {

	std::deque<double> TikhonovNumDiff (std::deque<double> x, std::deque<double> y, int k);
	double MedianFilter(std::deque<double> arr);
	double mean(std::deque<double> arr, int n);
	double standardDeviation(std::deque<double> arr, int n);
	double coefficientOfVariation(std::deque<double> arr, int n);

	//numerical differentiation using Tikhonov
	std::deque<double> TikhonovNumDiff (std::deque<double> x, std::deque<double> y, int k) 
	{
	  int n = x.size ();
	  if (n<1) n=1;
	  double a = x.front (), b = x.back ();
	  double dt = (b - a)/(double)n;
	  //std::cout << " " << std::endl << a << " " << b << " " << n << " " << dt << std::endl;
	  //double t[n];
	  std::vector<double> t(n);
	  Eigen::MatrixXd A(n, n);
	  Eigen::MatrixXd I(n, n);
	  Eigen::MatrixXd D1(n, n);
	  Eigen::MatrixXd D2(n,n);
	  Eigen::MatrixXd D(n,n);
	  Eigen::VectorXd y_hat(n);
	  Eigen::VectorXd y_(n);
	  Eigen::VectorXd u_(n);
	  Eigen::MatrixXd LHS(n,n);
	  Eigen::VectorXd RHS(n);
	  double reg_par = 0.01;

	  for (int j=0; j < n; j++)
	  {
	    t[j] = a + j*dt;
	  }

	  I.setIdentity ();

	  A.setZero ();
	  for (int i=0; i < n; i++) 
	  {
	    for (int j=1; j < n+1; j++) 
	    {
	      if (x[i] <= t[j-1]) A(i,j-1) = 0;
	      else if (t[j-1] < x[i] && x[i] < t[j]) A(i,j-1) = x[i] - t[j-1];
	      else if (t[j] <= x[i]) A(i,j-1) = dt;
	    }
	  } 

	  D1.setZero (); 
	  D1.diagonal(1).setConstant (1/(2*dt));
	  D1.diagonal(-1).setConstant (-1/(2*dt));
	  D1(0,0) = -1/(2*dt);
	  D1(n-1,n-1) = 1/(2*dt);

	  D2.setZero (); 
	  D2.diagonal(1).setConstant (1/(dt*dt));
	  D2.diagonal(0).setConstant (-2/(dt*dt));
	  D2.diagonal(-1).setConstant (1/(dt*dt));

	  if (k>2) k=2;
	  if (k<0) k=0;
	  if (k==2) D = I*D1.transpose ()*D2.transpose ();
	  else if (k==1) D = I*D1.transpose ();
	  else D = I;
	  for (int i=0; i < n; i++) 
	  {
	    y_hat(i) = y[i] - y[0];
	    y_(i) = y[i];
	  }   

	  LHS = A.transpose ()*A + reg_par*D.transpose ()*D;
	  RHS = A.transpose ()*y_hat;

	  u_ = LHS.inverse () * RHS;

	  std::deque<double> u;
	   for (int i=0; i < n; i++) 
	   {
	     //if ( isfinite(u_(i)) ) u.push_back (u_(i));
	     u.push_back (u_(i));
	   }   

	  return u;
	}

	/* Function to sort an array using insertion sort*/
	double MedianFilter(std::deque<double> arr)
	{
	    int i, key, j;
	    int n = arr.size ();
	    for (i = 1; i < n; i++)
	    {
	        key = arr[i];
	        j = i - 1;
	 
	        /* Move elements of arr[0..i-1], that are
	        greater than key, to one position ahead
	        of their current position */
	        while (j >= 0 && arr[j] > key)
	        {
	            arr[j + 1] = arr[j];
	            j = j - 1;
	        }
	        arr[j + 1] = key;
	    }

	    return arr[std::ceil(n/2)];
	}

	// Function to find mean of given array.
	double mean(std::deque<double> arr, int n)
	{
	    float sum = 0;
	    for (int i = 0; i < n; i++)
	        sum = sum + arr[i];
	    return sum / n;
	}

	// Function to find standard deviation
	// of given array.
	double standardDeviation(std::deque<double> arr, int n)
	{
	    float sum = 0;
	    for (int i = 0; i < n; i++)
	        sum = sum + (arr[i] - mean(arr, n)) *
	                    (arr[i] - mean(arr, n));
	 
	    return sqrt(sum / (n - 1));
	}

	// Function to find coefficient of variation.
	double coefficientOfVariation(std::deque<double> arr, int n)
	{
	   return standardDeviation(arr, n) / mean(arr, n);
	}


}


#endif /* MY_MATHS_TOOLS_H */
