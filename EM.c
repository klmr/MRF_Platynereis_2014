/**************************************************************************************************
 * Jean-Baptiste Pettit
 * European Bioinformatics Institute
 * 
 * NOTICE OF LICENSE
 *
 * This source file is subject to the Academic Free License (AFL 3.0)
 * that is bundled with this package in the file LICENSE_AFL.txt.
 * It is also available through the world-wide-web at this URL:
 * http://opensource.org/licenses/afl-3.0.php.
****************************************************************************************************/

/**********************************BASIC USAGE EXPLANATIONS******************************************
* A prettier explanation can be found in the wiki of the github page of the project here:
* https://github.com/jbogp/MRF_Platynereis_2014/wiki
*
####### COMPILING THE CODE #######
The implementation is written in C. a make file is available but has only been tested on a Linux platform. If you are having troubles compiling in other environnement please report an issue on the project Github page (https://github.com/jbogp/MRF_Platynereis_2014/issues)

####### REQUIRED FILES #######
To function, the algorithm requires at least 2 files :
 - The dataset file which contains binarised expression data for each considered point with the following organisation:
   - Each line is a datapoint
   - The first column is a unique identifier for each line
   - The next M columns are the binary gene expression for all the genes considered (0 if not expressed 1 if expressed)
   - The charater to separate columns is a tabulation
   - Note that no missing data is allowed and all points must have expression information for all genes
   
 - The neighbouring graph file :
   - Each line is a datapoint (in the same order as the dataset file)
   - The first colums represents the number of neighbours (u) for this data point
   - The u next columns represent the line number of the neighbours. For example if the point on line 1 has 3 neughbours, the points on line 5, 7 and 12, the file should start with : `3	5	7	12`
   
####### INITIALIZATION FILE #######
This file is not required as a random intialization can be generated by the algorithm (see the PARAMETERS section for details). However if you wish to initialize the clustering with a specific classification, the format is the follwing :
- Each line is a datapoint (in the same order as the dataset file)
- Each line has only one column that is a number between 1 and K (number of desired clusters) corresponding to the clusters in with the point should initialy be clustered in.

####### PARAMETERS INPUT ########
The parameters for the algorithm are as follow [parameter name] indicates the parameter is mandatory {parameter name} indicates it's optional [param 1 | param2] indicates a choice
./EM [path to data_file] [path neighbouring file] ["rand" | path to initialisation file] [initial value for beta] [number of clusters K] [result folder] [outputFileName] [number of clusters changed from one iteration to the next to assume convergence] {"fixed" (if present, beta will be fixed to initial value instead of being estimated)}

####### OUTPUT FILES #######
The algorithm produces 4 files when convergence is reached
- outputFileName.csv contains the clustering results in the same format as the initialization file
- outputFileName.thetas contains a table with the final values of the theta parameters
- outputFileName.clust contains a table with the final value of beta and the number of points for each cluster
- outputFileName.summary contains information about the run, namely the numbers of clusters, the final log likelihood value and the number of iterations needed to reach convergence
****************************************************************************************************/

#include "EM.h"


/********************************************************************************************/
/**********************************START Defining methods*****************************************/

/*Parses one line, places numbers un int vector*/
int parse_vect(char * line, int * res) {
	int i=0;
	char delims[] = "\t";
	char *result = NULL;
	result = strtok( line, delims );
	while( result != NULL) {
		res[i] = atoi(result);
		result = strtok( NULL, delims );
		i++;
	}
	return i;
}


/*Reading and storing binary file data
returns : void
*/
void load_data(FILE * data /*I*/,dataSet * myData/*I\O*/) {
	/*declarations*/
	int i=0;
	char line[LG_LIG_MAX];
	int length;

	myData->obs = (dataPoint *)malloc(sizeof(dataPoint)*LG_LIG_MAX);

	/*instructions*/
	while(fgets(line,LG_LIG_MAX+1,data) != NULL) {	
		(myData->obs)[i].expVect = (int *)malloc(sizeof(int)*LG_GENES_MAX);
		length = parse_vect(line,(myData->obs)[i].expVect);			
		i++;
	}

	/*creating return variable*/
	myData->length = length;
	myData->num = i;

}

/*Parses neighbouring files, first number is number of neighbours then indexes*/
int parse_nei(char * line, int * res) {
	int i=0;
	char delims[] = "\t";
	char *result = NULL;
	int length;


	result = strtok( line, delims );
	while( result != NULL) {
		if(i == 0) {
			length = atoi(result);
		}
		else{
			res[i-1] = atoi(result)-1;

		}
		result = strtok( NULL, delims );
		i++;
	}
	return length;
}


/*Reading and storing spatial information
returns : void
*/
void load_nei(FILE * data /*I*/,dataSet * myData/*I\O*/) {
	/*declarations*/
	int i=0;
	char line[LG_NEI_MAX];
	int length;

	/*instructions*/
	while(fgets(line,LG_LIG_MAX+1,data) != NULL) {	
		(myData->obs)[i].nei = (int *)malloc(sizeof(int)*LG_NEI_MAX);
		length = parse_nei(line,(myData->obs)[i].nei);	

		(myData->obs)[i].numNei = length;		
		i++;
	}
		

}


/*Initialize classification randomly
*
*The default number of random initialization is 10 (the one with the best initial likelihood is used to proceed), to modify it change the `numRand` parameter and recompile the code
returns void*/
void initClassifRand(dataSet set/*I*/,int numClust/*I*/, classif* myClassif/*I\O*/,double beta) {
	int i,k;
	long double logLike,bestLogLike;
	int * bestClust;
	int numRand = 10;
	/*initializing parameters and setting set*/

	myClassif->set = set;
	myClassif->numClust = numClust;

	srand((unsigned)time(NULL));
	myClassif->beta = (double *)malloc(sizeof(double)*myClassif->numClust);
	myClassif->clust = (int *)malloc(sizeof(int)*set.num);
	myClassif->parameters.theta = (double **)malloc(sizeof(double*)*numClust);

	/*Init tihms to 0??*/
	myClassif->tihm = (long double **)malloc(sizeof(long double*)*myClassif->numClust);
	/*Init cell densities*/
	myClassif->cellDensities = (long double **)malloc(sizeof(long double*)*myClassif->numClust);
	
	/*iter on clusters*/
	for(k=0;k<numClust;k++) {
		myClassif->tihm[k] = (long double *)malloc(sizeof(long double)*myClassif->set.num);
		myClassif->cellDensities[k] = (long double *)malloc(sizeof(long double)*myClassif->set.num);
		for(i=0;i<myClassif->set.num;i++){
			myClassif->tihm[k][i] = 0;
		}
		myClassif->parameters.theta[k] = (double *)malloc(sizeof(double)*(set.length));
		myClassif->beta[k] = beta;
	}


	/*Allocating memory for best classif*/
	bestClust = (int *)malloc(sizeof(int)*set.num);
	bestLogLike = -1e100;

	/*Generate x random classif and take the one with the best likelihood*/
	for(k=0;k<numRand;k++) {


		/*iter on data points*/
		for(i=0;i<set.num;i++) {
			myClassif->clust[i] = rand()%numClust+1;
		}

		/*Computing thetas*/
		maxThetas(myClassif);
		/*Computing cell densitites*/
		computeCellDensities(myClassif);
		/*computing tihms with 2 step fixed point*/
		computeThims(myClassif,2);
		
		/*Computing loglikelihood*/
		logLike = computeFullLogLikelihood(myClassif);
		if(logLike > bestLogLike) {
			bestLogLike = logLike;
			memcpy(bestClust,myClassif->clust,(set.num)*sizeof(int)); 
		}
		printf("Init %d likelihood : %Le\n",k,bestLogLike);
	}
	
	/*Assigning best random classif, no need to recompute thetas and thims, it will be done at first iteration*/
	memcpy(myClassif->clust,bestClust,(set.num)*sizeof(int)); 


	/*Computing thetas*/
	maxThetas(myClassif);
	/*Computing cell densities*/
	computeCellDensities(myClassif);
	/*computing tihms with 1 step fixed point*/
	computeThims(myClassif,2);
	printf("\tBest init has a likelihood of %Le\n",bestLogLike);
	
	free(bestClust);

	
}

/*Initialize classification from file
returns void*/
void initClassifFile(dataSet set/*I*/,FILE * data, classif* myClassif/*I\O*/,double beta) {
	int i,k,maxClust=0;
	char line[LG_NEI_MAX];


	/*initializing parameters and setting set*/
	myClassif->set = set;

	srand((unsigned)time(NULL));
	myClassif->clust = (int *)malloc(sizeof(int)*set.num);

	i=0;

	/*reading cluters file*/
	while(fgets(line,LG_LIG_MAX+1,data) != NULL) {	
		if(atoi(line) > maxClust){
			maxClust = atoi(line);
		}	
		myClassif->clust[i] = atoi(line);
		i++;

	}
	myClassif->numClust = maxClust;
	printf("clusters :%d\n",myClassif->numClust);

	myClassif->beta = (double *)malloc(sizeof(double)*myClassif->numClust);
	myClassif->parameters.theta = (double **)malloc(sizeof(double*)*myClassif->numClust);

	/*Init tihms to 0??*/
	myClassif->tihm = (long double **)malloc(sizeof(long double*)*myClassif->numClust);
	/*Init cell densities*/
	myClassif->cellDensities = (long double **)malloc(sizeof(long double*)*myClassif->numClust);
	
	/*iter on clusters*/
	for(k=0;k<myClassif->numClust;k++) {
		myClassif->tihm[k] = (long double *)malloc(sizeof(long double)*myClassif->set.num);
		myClassif->cellDensities[k] = (long double *)malloc(sizeof(long double)*myClassif->set.num);
		for(i=0;i<myClassif->set.num;i++){
			myClassif->tihm[k][i] = 0;
		}
		myClassif->parameters.theta[k] = (double *)malloc(sizeof(double)*(set.length));
		myClassif->beta[k] = beta;
	}
	printf("thims\n");

	/*Computing thetas*/
	maxThetas(myClassif);
	/*Computing cell densities*/
	computeCellDensities(myClassif);
	/*computing tihms with 2 step fixed point*/
	computeThims(myClassif,2);

}

/*Compute cell density for one cell one cluster*/
long double cellDensity(classif * myClassif/*i*/,int clust/*I*/,int cell/*I*/) {
	int j;
	long double density=0;
	long double ret=0;
	
	/*Iter on genes*/
	for(j=1;j<myClassif->set.length;j++) {
		if(myClassif->set.obs[cell].expVect[j] == 1) {
			density = logl((long double)myClassif->parameters.theta[clust][j]);
		}
		else {
			density = logl((long double)1.0-myClassif->parameters.theta[clust][j]);
		}
		ret = ret+density;
		
	}



	return ret;

}

/*Computes all cells densities*/
void computeCellDensities(classif * myClassif/*I/O*/) {
	int i,k;	
	/*Iter on cells*/
	for(i=0;i<myClassif->set.num;i++) {
		/*Iter on clusters*/
		for(k=0;k<myClassif->numClust;k++){
			myClassif->cellDensities[k][i] = cellDensity(myClassif,k,i);
		}
		
	}

}

/*function to check if all elements of the count vector > 0
* returns 1 if vector contains 0s 0 otherwise
*/
int zerosInVector(int * vector,int length) {
	int zeros = 0,index=0;
	while(zeros == 0 && index<length) {
		if(vector[index] == 0) {
			zeros = 1;
		}
		index++;
	}
	return zeros;
}

/*Checks if current classif has at least one point in each cluster
*If not it displays a warning.
*Empty classes usually lead to NaN final likelihood, if you get that error a lot try initializing the the clutering differently
*
*/
void noEmptyClass(classif * myClassif) {
	int * numTot;
	int i;

	/*Allocating memory*/
	numTot = (int *)malloc(sizeof(int)*myClassif->numClust);
	

	/*initialize present to 0*/
	for(i=0;i<myClassif->numClust;i++) {
		numTot[i] = 0;
	}

	/*Filling the count vector in all laziness*/
	while(zerosInVector(numTot,myClassif->set.num) == 1 && i<myClassif->set.num) {
		numTot[myClassif->clust[i]]++;
		i++;
	}

	for(i=0;i<myClassif->numClust;i++) {
		/*If empty class, alert*/
		if(numTot[i+1] == 0) {
			printf("\tWARNING : class %d is empty\n",i+1);
		}
	}

}





/*Returns the neighboring for one cell and one cluster*/
long double computeNeiCoef(classif * myClassif/*i\O*/,long double ** currentT/*I*/,int clust/*I*/,int cell/*I*/){
	int j;
	long double temp=0;
	long double ret=0;


	for(j=0;j<myClassif->set.obs[cell].numNei;j++) {
		temp = temp + currentT[clust][myClassif->set.obs[cell].nei[j]];
	}
	
	ret = temp;

	return ret;
}

/*Returns Rz, the neighborhood factor to the model likelihood*/
long double computeNeiLikelihood(classif * myClassif/*i\O*/,int cell/*I*/,int clust/*I*/){
	int j;
	long double ret=0;
	
	/*Iter on cell neighbors*/
	for(j=0;j<myClassif->set.obs[cell].numNei;j++) {
		/*If neighbor is in the same cluster than considered cell, increment*/
		if(myClassif->clust[myClassif->set.obs[cell].nei[j]] == clust) {
			ret++;
		}
	}

	return ret;	
}


/*Computes expectation*/
long double computeFullExpectation(classif * myClassif/*I*/) {
	int i,k;
	long double logLikeRx=0.0,logLikeRz;
	/*Checking for empty classes*/
	noEmptyClass(myClassif);
	/*Iter on cells*/
	for(i=0;i<myClassif->set.num;i++) {
		/*Iter on clusters*/
		for(k=0;k<myClassif->numClust;k++){
			/*Iter on genes*/
			if(myClassif->tihm[k][i] != (long double)0) {
				logLikeRx = logLikeRx+((myClassif->cellDensities[k][i])*myClassif->tihm[k][i]);
			}			
		}
	}
	logLikeRz = computeBetaExpectation(myClassif);
	/*return value*/
	return logLikeRx+logLikeRz;
}

/*Computes the beta part of the expected likelihood*/
long double computeBetaExpectation(classif * myClassif/*I*/) {
	long double expectation = 0,numerator,denominator=0;
	int i,k;
	
	/*summing over the cells to compute denominator*/
	for(i=0;i<myClassif->set.num;i++) {
		denominator = 0;
		/*Iter on clusters to compute denominator*/
		for(k=1;k<=myClassif->numClust;k++){
			denominator += expl(myClassif->beta[k-1]* computeNeiLikelihood(myClassif,i,k));


		}
		numerator = expl(myClassif->beta[(myClassif->clust[i]-1)]*computeNeiLikelihood(myClassif,i,myClassif->clust[i]));

		/*Iter on possible clusters*/
		for(k=1;k<=myClassif->numClust;k++){
			expectation += myClassif->tihm[k-1][i]*logl(numerator/denominator);
		}
	}

	return expectation;
}

/*Computes logLikelihood*/
long double computeFullLogLikelihood(classif * myClassif/*I*/) {
	int i;
	long double logLikeRx=0.0,logLikeRz;
	/*Checking for empty classes*/
	noEmptyClass(myClassif);
	/*Iter on cells*/
	for(i=0;i<myClassif->set.num;i++) {
		logLikeRx += myClassif->cellDensities[myClassif->clust[i]-1][i];			
	}
	logLikeRz = computePseudoLogLikelihood(myClassif);
	/*return value*/
	return logLikeRx+logLikeRz;
}

/*compute model pseudo-logLikelihood*/
long double computePseudoLogLikelihood(classif * myClassif/*I*/) {
	long double pseudoLike = 0,numerator,denominator=0;
	int i,k;
	
	/*summing over the cells to compute denominator*/
	for(i=0;i<myClassif->set.num;i++) {
		denominator = 0;
		/*Iter on clusters to compute denominator*/
		for(k=1;k<=myClassif->numClust;k++){
			denominator += expl(myClassif->beta[k-1]* computeNeiLikelihood(myClassif,i,k));
		}

		numerator = expl(myClassif->beta[(myClassif->clust[i]-1)]*computeNeiLikelihood(myClassif,i,myClassif->clust[i]));
		pseudoLike += logl(numerator/denominator);
	}

	return pseudoLike;
}


/*Computes current thims with a fixed point algorithm.
*
* The number of iteration of the fixed point algorithm is set through the numIterFixed parameter
*/
void computeThims(classif * myClassif /*I/O*/, int numIterFixed /*I*/) {
	int k,i,iterFixed;
	long double sumDivisor;
	long double * neiCoef;
	long double temp;
	

	/*Allocating memory*/
	neiCoef = (long double *)malloc(sizeof(long double)*myClassif->numClust);

	/* Checking if mem alloc went OK*/
	if (neiCoef != NULL) {
		/*computing thims*/
		for(iterFixed=0;iterFixed<numIterFixed;iterFixed++) {

			for(i=0;i<myClassif->set.num;i++) {

				sumDivisor = 0.0;

				/* Computing all cell densities*/
				for(k=0;k<myClassif->numClust;k++) {

					neiCoef[k] = expl(computeNeiCoef(myClassif,myClassif->tihm,k,i)*(long double)myClassif->beta[k]);

					sumDivisor = sumDivisor+(expl(myClassif->cellDensities[k][i])*neiCoef[k]);


				}
				for(k=0;k<myClassif->numClust;k++) {
					temp = (expl(myClassif->cellDensities[k][i])*neiCoef[k])/sumDivisor;
					myClassif->tihm[k][i] = temp;	
				}



			}

			
		}
	}

	/*Freeing memory*/
	free(neiCoef);
	neiCoef = NULL;

	return;
			
}





/*Gradient descent algorithm to maximize beta paramters sequentially*/
void gradientAscent(classif * myClassif /*I/O*/) {
	long double derivatePlus,derivateMinus;
	double init_step = 0.1;
	double step = init_step;
	long double currentLike = 0;
	double oriBeta;
	int stopAscent =0;
	int k;
	

	/*for each beta*/
	for(k=0;k<myClassif->numClust;k++) {
		derivatePlus = -100;
		derivateMinus = -100;
		step = init_step;
		currentLike = 0;
		stopAscent = 0;
		/*varying beta until convergence to local maximum*/
		while(stopAscent <1) { 
			/*Registering current Beta*/
			oriBeta = myClassif->beta[k];
			/*numerical derivate*/
			currentLike = 	computeBetaExpectation(myClassif);
			/*Computing derivate */
			myClassif->beta[k] = oriBeta +step;
			derivatePlus = computeBetaExpectation(myClassif)-currentLike;
			myClassif->beta[k] = oriBeta - step;
			derivateMinus = computeBetaExpectation(myClassif)-currentLike;
			/*which way to go?*/
			if(derivatePlus > 0 || derivateMinus > 0) {
				if(derivatePlus > 0 && derivatePlus>derivateMinus) {
					myClassif->beta[k] = oriBeta +step;
				}
				else if((oriBeta -step) > 0) {
					myClassif->beta[k] = oriBeta -step;
				}
				else {
					myClassif->beta[k] = 0.0;
					stopAscent = 1;
				}
			}
			else {
				myClassif->beta[k] = oriBeta;
				stopAscent = 1;	
			}

			/*Uncomment for verbose mode on the gradient ascent*/
			/*printf("cluster : %d | derivPlus: %Le | derivMinus: %Le | like : %Le | beta : %f | step : %f\n",k,derivatePlus,derivateMinus,currentLike,myClassif->beta[k],step);*/
		}
	}
	

}




/*E Step of the EM algorithm
*Returns the number of clusters assignments that have changed compared to the previous clustering
*this value is used to know if convergence has been reached
*/
int eStep(classif * myClassif /*I\O*/) {
	int k,i,hasConverged=0;
	long double maxClust;
	int clustK;
	
	/*Computing new cell densities*/
	computeCellDensities(myClassif);
	/*Computing thims with fixed point algo with 3 steps*/
	computeThims(myClassif,3);
	

	/*assigning cells to cluster*/
	for(i=0;i<myClassif->set.num;i++) {
		maxClust = -1000;
		clustK=1;
		for(k=0;k<myClassif->numClust;k++) {
			if((myClassif->tihm[k][i]-maxClust) > 0.0) {
				maxClust = myClassif->tihm[k][i];
				clustK = k+1;

			}
		}

		/*Assigning Cluster !*/
		if(myClassif->clust[i] != clustK ){
				hasConverged++;
				myClassif->clust[i] = clustK;
		}


	}

	return hasConverged;
}


/*returns the numbers of cells having similarly expressed genes as the exp one in the same cluster*/
int numCellsAlike(int exp,int indexGene,int clust,classif * myClassif) {
	int numCells=0,i;
	for(i=0;i<myClassif->set.num;i++) {
		if(exp == myClassif->set.obs[i].expVect[indexGene] && clust == myClassif->clust[i]){
			numCells++;
		}
	}
	return numCells;
}

/*returns number of cells for one cluster*/
int numCellsClust(int clust,classif * myClassif) {
	int numCells=0,i;
	for(i=0;i<myClassif->set.num;i++) {
		if(clust == myClassif->clust[i]){
			numCells++;
		}
	}
	return numCells;
}


/*Maximize thetas*/
void maxThetas(classif * myClassif /*I/O*/) {
	double numtheta;
	double dentheta;	
	int k,j;


	/*Iter on clusters*/
	for(k=0;k<myClassif->numClust;k++) {
		/*Iter on genes starting at 1 because first value is cell ID !!!*/
		for(j=1;j<myClassif->set.length;j++){
			numtheta =  (double)numCellsAlike(1,j,k+1,myClassif);
			dentheta = (double)numCellsClust(k+1,myClassif);

			myClassif->parameters.theta[k][j] = (numtheta/dentheta);

		}		
			
	}
}

/*M step of the algorithm*/
void mStep(classif * myClassif,int type_beta) {
	
	/*Maximizing thetas*/
	maxThetas(myClassif);
	if(type_beta == 0){
		/*Maximizing betas*/
		gradientAscent(myClassif);
	}

	
}




/**********************************END Defining nethods*****************************************/
/*******************************************************************************************/
/**********************************Result analysis functions*****************************************/

/*Outputs he clustering results*/
void outputCSV(char * out, classif * myClassif) {
	int i;
	FILE * file;	

	file = fopen(out,"w");
	for(i = 0;i<myClassif->set.num;i++) {
		fprintf(file,"%d\n",myClassif->clust[i]);
	}
	fclose(file);
}

/*Outputs the final values for the theta parameters*/
void outputThetas(char * out,classif * myClassif) {
	int i,k;
	FILE * file;	

	file = fopen(out,"w");
	for(k = 0;k<myClassif->numClust;k++) {
		for(i = 1;i<myClassif->set.length;i++) {
			fprintf(file,"%f\t",myClassif->parameters.theta[k][i]);
		}
		fprintf(file,"\n");
	}
	fclose(file);
}


/*Outputs EM summary for further analysis*/
void outputEMSummary(char * out,classif * myClassif,int numIter) {
	long double likely;
	FILE * file;	

	file = fopen(out,"w");
	likely = computeFullExpectation(myClassif);
	fprintf(file,"numClust\t%d\n",myClassif->numClust);
	fprintf(file,"likelihood\t%Le\n",likely);
	fprintf(file,"Iterations\t%d\n",numIter);
	fclose(file);
} 


/*Outputs EM summary for each cluster*/
void outputClustSummary(char * out,classif * myClassif) {
	FILE * file;
	int k;	

	file = fopen(out,"w");
	
	/*Outputing header*/
	fprintf(file,"\t");
	for(k=0;k<myClassif->numClust;k++) {
		fprintf(file,"cluster_%d\t",(k+1));
	}
	fprintf(file,"\n");
	
	/*Outputing betas*/
	fprintf(file,"beta");
	for(k=0;k<myClassif->numClust;k++) {
		fprintf(file,"\t%f",myClassif->beta[k]);
	}
	fprintf(file,"\n");
	
	/*Outputing num per clust*/
	fprintf(file,"numCells");
	for(k=1;k<=myClassif->numClust;k++) {
		fprintf(file,"\t%d",numCellsClust(k,myClassif));
	}
	fprintf(file,"\n");
	
	
	fclose(file);
}



/**********************************END Result analysis functions*****************************************/
/*******************************************************************************************/
/**********************************START MAIN Function*****************************************/

/*Kill handling function*/
void killHandle (int sig) {
	printf("Program interrupted...exiting\n");
	exit(0);
	
}


int main(int argc, char* argv[]) {

	/* variables declarations*/
	/* File handles  and names*/
	FILE *fbinarized;
	FILE *fnei;
	FILE *finit;
	char * summaryFile;
	char * clustFile;
	char * csvFile;
	char * thetaFile;
	
	/*Dataset variable*/
	dataSet fullData;
	
	/*Classification object*/
	classif clusters;
	
	int j=1,k;
	int hasConverged;
	int convergeLimit;
	int limitIter = 100;

	mode_t process_mask;
	int type_beta=0;



	/*Handling kill signals to output current results on kill*/
	signal (SIGQUIT, killHandle);
	signal (SIGINT, killHandle);
	
	/* Start*/
	/*checking command*/
	if(argc < 7) {
		printf("Wrong command, syntax is : [path to data_file] [path neighbouring file] ['rand' | path to initialisation file] [initial value for beta] [number of clusters K] [result folder] [outputFileName] [number of clusters changed from one iteration to the next to assume convergence] {'fixed' (if present, beta will be fixed to initial value instead of being estimated)}\n");
	}
	else {
	
		/*Set required parameters*/
		hasConverged=atoi(argv[8])+1;
		convergeLimit=atoi(argv[8]);

		/* open and load data file */
		fbinarized = fopen(argv[1],"r");
		load_data(fbinarized,&fullData);
		fclose(fbinarized);

		/* open and load spatial info */
		fnei = fopen(argv[2],"r");
		load_nei(fnei,&fullData);
		fclose(fnei);

		/* Data is loaded and stored */

		printf("Starting initialisation\n");
		/* INITIALIZATION STEP */
		if(strncmp(argv[3],"rand",100) == 0){
			printf("Random Initialisation\n");
			initClassifRand(fullData,atoi(argv[5]),&clusters,atof(argv[4]));
		}
		else{
			finit = fopen(argv[3],"r");
			printf("Initialisation from file : %s\n",argv[3]);
			initClassifFile(fullData,finit,&clusters,atof(argv[4]));
			fclose(finit);
		}

		/*detecting beta type*/
		if(argv[9] != NULL && strncmp(argv[9],"fixed",100) == 0){
			type_beta = 1;
		}

		/*Initializing output*/
		process_mask = umask(0);
		mkdir(argv[6], S_IRWXU | S_IRWXG | S_IRWXO);
		umask(process_mask);

		printf("Initialisation done.\n");
		/* END INITIALIZATION */
		printf("\n");
		printf("\n");


		/* WHILE not converged*/
		printf("Starting EM process\n");
		while(hasConverged > convergeLimit && limitIter-->=0){

			/* E-Step */
			printf("\tE Step, estimating\n");
			hasConverged =eStep(&clusters);

			/* Compute thetas based on random classification */
			printf("\tM Step, maximazing parameters\n");
			mStep(&clusters,type_beta);

			printf("\tCurrent beta values :");
			for(k=0;k<clusters.numClust;k++) {
				printf(" %f",clusters.beta[k]);
			}
			printf("\n");

			printf("\tClusters changed : %d\n\tCurrent Likelihood :%Le\n",hasConverged,computeFullLogLikelihood(&clusters));


			if(hasConverged <= convergeLimit) {
				printf("Converges !");
			}
			printf("\n\n");
			j++;
		}
		printf("Creating output\n");



		/*Outputing results*/

		summaryFile = malloc(snprintf(NULL, 0, "%s/%s.summary", argv[6], argv[7]) + 9);
		sprintf(summaryFile, "%s/%s.summary", argv[6], argv[7]);
		outputEMSummary(summaryFile,&clusters,j);


		csvFile = malloc(snprintf(NULL, 0, "%s/%s.csv", argv[6], argv[7]) + 5);
		sprintf(csvFile, "%s/%s.csv", argv[6], argv[7]);
		outputCSV(csvFile,&clusters);

		thetaFile = malloc(snprintf(NULL, 0, "%s/%s.theta", argv[6], argv[7]) + 7);
		sprintf(thetaFile, "%s/%s.theta", argv[6], argv[7]);
		outputThetas(thetaFile,&clusters);
		
		
		clustFile = malloc(snprintf(NULL, 0, "%s/%s.clust", argv[6], argv[7]) + 7);
		sprintf(clustFile,"%s/%s.clust", argv[6], argv[7]);
		outputClustSummary(clustFile,&clusters);

	}



	return 1;
	
}
/**********************************END MAIN and END File*****************************************/
