#include "Cache.h"
#include "GProcessor.h"
#include "GMemorySystem.h"
#include "MemObj.h"
#include "MemorySystem.h"
#include "Processor.h"
#include "xuStats.h"
#include "OSSim.h"
#include "ProcessId.h"
xuStats::xuStats(char *fileName,char *totFileName,int nCPUs){
    this->fileName    = fileName;
    this->totFileName = totFileName;
    fp    = NULL;
    totFp = NULL;
    preData  = new statsData[nCPUs];
    thisData = new statsData[nCPUs];
    for(int i = 0; i < nCPUs; i++){
       preData[i].dl1miss        = 0;
       preData[i].il1miss        = 0;
       preData[i].l2miss         = 0;
       preData[i].dl1hit         = 0;
       preData[i].il1hit         = 0;
       preData[i].l2hit          = 0;
       preData[i].itlb           = 0;
       preData[i].dtlb           = 0;
       preData[i].nbranche       = 0;
       preData[i].nbranchMiss    = 0;
       preData[i].nbranchHit     = 0;
       preData[i].energy         = 0.0;
       preData[i].clock          = 0;
       memset(preData[i].nInst,0,MaxInstType * sizeof(long long));
       memset(preData[i].nStall,0,MaxStall * sizeof(long long));
       preData[i].sumInst        = 0;
       preData[i].IPC            = 0.0;
       preData[i].energyEffiency = 0.0;
       
       context[i].sumInst        = 0;
       context[i].clock          = 0;
       context[i].IPC            = 0;
       context[i].power          = 0;
       context[i].energy         = 0;
       context[i].pw             = 0;
	
    }
    for(int i = 0; i < xu_nCPU; i++){
    	for( int j = 0; j < control_NUM + 1; j++){
	    control_table[i][j] = -1;
	}
	control_table[i][temp_NUM] = 0;
    }
    for(int i = 0; i < xu_nThread; i++){
	    statInst[i] = 0;
    }


    for(int i = 0; i < xu_nThread; i++){
	    sigma_curr[i] = i;
	    is_Done[i] = false;
	    is_Free[i] = false;
    	    pre[i]     = false;
    }

    for(int i = 0; i < xu_nThread; i++){
	    sigma_dst[i] = i;
    }

    for(int i = 0; i < xu_nCPU; i++){
	    arc_sigma[i] = i;
    }
     memset(&phase_table[0][0][0],0,xu_nThread * xu_nPhase * 4 * sizeof(long long));
     memset(&phase_pw[0][0][0],0,xu_nThread * xu_nPhase * xu_nCPU * sizeof(double));
     memset(&phase_Z[0][0],0,xu_nThread * 4 * sizeof(long long));
     memset(&statInst[0],0,xu_nThread * sizeof(long long));

     totalNInst = 100000000;
     interval   = 100000;
     ITV_diff   = 0.050;
     nMigrate   = 0;
     nBegin     = 0;
}

void xuStats::getStatData(GProcessor *core){
    int   cpuId         = core->getId();
    int   threadId      = arc_sigma[cpuId];
    GMemorySystem *gm   = core->getMemorySystem();
    MemObj *dataSource  = gm->getDataSource();  //dl1
    MemObj *instSource  = gm->getInstrSource(); //il1
    MemObj *l2Cache     = (*(instSource->getLowerLevel())).front();
    BPredictor *bpred   = core->xuGetBPred();

   // get instructions    
    GStatsCntr **ppnInst    = core->xuGetNInst(); 
    thisData[cpuId].sumInst = 0;
    for(int i  = iOpInvalid; i < MaxInstType; i++){ //the number of instruction per  type 
	int temp                  = ppnInst[i]->getValue(); 
	thisData[cpuId].nInst[i]  = temp;
    	thisData[cpuId].sumInst  += temp; ///the number of instruction for IPC
    }
    //get stall
    GStatsCntr **ppnStall  = core->xuGetNStall();
    for(int i  = NoStall + 1; i < MaxStall; i++){
    	thisData[cpuId].nStall[i]  = ppnStall[i]->getValue();
    }
    //get dl1
    dataSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].dl1miss  = thisData[cpuId].missValue;
    thisData[cpuId].dl1hit   = thisData[cpuId].hitValue;
    
    //get il1
    instSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].il1miss  = thisData[cpuId].missValue;
    thisData[cpuId].il1hit   = thisData[cpuId].hitValue;
    //get clock for IPC
    thisData[cpuId].clock    = core->getClockTicks();   //clock
  
    //get l2 cache miss
    l2Cache->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].l2miss   = thisData[cpuId].missValue;
    thisData[cpuId].l2hit    = thisData[cpuId].hitValue;
    
    //get itlb
    thisData[cpuId].itlb     = gm->getMemoryOS()->xuGetITLBMiss()->getValue();    
    //get dtlb  
    thisData[cpuId].dtlb     = gm->getMemoryOS()->xuGetDTLBMiss()->getValue();    
    //bpred
    thisData[cpuId].nbranche     = bpred->xuGetNBranche();
    thisData[cpuId].nbranchMiss  = bpred->xuGetNBrancheMiss();
    thisData[cpuId].nbranchHit   = thisData[cpuId].nbranche - thisData[cpuId].nbranchMiss;


    //get energy
    const char * coreName  = SescConf->getCharPtr("","cpucore",cpuId); 
    double pPower          = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
    double maxClockEnergy  = EnergyMgr::get(coreName,"clockEnergy",cpuId);
    double maxEnergy       = EnergyMgr::get(coreName,"totEnergy");
    // More reasonable clock energy. 50% is based on activity, 50% of the clock
    // distributtion is there all the time
    double clockPower      = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
    double corePower       = pPower + clockPower;
    double coreEnergy      = EnergyMgr::ptoe(corePower);
    thisData[cpuId].energy = coreEnergy;
}

bool xuStats::solveThisData(GProcessor *core){
    int cpuId    = core->getId();
    int threadId = arc_sigma[cpuId];

    //performance event
    long long diffdl1miss     = thisData[cpuId].dl1miss - preData[cpuId].dl1miss;
    long long diffil1miss     = thisData[cpuId].il1miss - preData[cpuId].il1miss;
    long long diffl2miss      = thisData[cpuId].l2miss  - preData[cpuId].l2miss;

    long long diffdl1hit      = thisData[cpuId].dl1hit - preData[cpuId].dl1hit;
    long long diffil1hit      = thisData[cpuId].il1hit - preData[cpuId].il1hit;
    long long diffl2hit       = thisData[cpuId].l2hit  - preData[cpuId].l2hit;
    long long diffitlb        = thisData[cpuId].itlb  - preData[cpuId].itlb;
    long long diffdtlb        = thisData[cpuId].dtlb  - preData[cpuId].dtlb;
    long long diffclock       = thisData[cpuId].clock - preData[cpuId].clock;
    long long diffinst        = thisData[cpuId].sumInst - preData[cpuId].sumInst;
    double    diffIPC         = (double)diffinst / (double)diffclock;
    double    diffenergy      = thisData[cpuId].energy - preData[cpuId].energy;
    double    diffpower       = diffenergy / diffclock*(osSim->getFrequency()/1e9);
    double    L1MissRate      = (double)(diffdl1miss + diffil1miss)/(double)(diffdl1miss + diffil1miss + diffdl1hit + diffil1hit);
    double    L2MissRate      = (double)diffl2miss/(double)(diffl2miss + diffl2hit);
    long long diffNBranch     = thisData[cpuId].nbranche - preData[cpuId].nbranche;
    long long diffNBranchMiss = thisData[cpuId].nbranchMiss - preData[cpuId].nbranchMiss;
    long long diffNBranchHit  = thisData[cpuId].nbranchHit - preData[cpuId].nbranchHit;
  ///Stall 
    long long diffWinStall          = thisData[cpuId].nStall[SmallWinStall]     - preData[cpuId].nStall[SmallWinStall]; 
    long long diffROBStall          = thisData[cpuId].nStall[SmallROBStall]     - preData[cpuId].nStall[SmallROBStall];  
    long long diffREGStall          = thisData[cpuId].nStall[SmallREGStall]     - preData[cpuId].nStall[SmallREGStall]; 
    long long diffLoadStall         = thisData[cpuId].nStall[OutsLoadsStall]    - preData[cpuId].nStall[OutsLoadsStall]; 
    long long diffStoreStall        = thisData[cpuId].nStall[OutsStoresStall]   - preData[cpuId].nStall[OutsStoresStall]; 
    long long diffBranchStall       = thisData[cpuId].nStall[OutsBranchesStall] - preData[cpuId].nStall[OutsBranchesStall]; 
    long long diffReplayStall       = thisData[cpuId].nStall[ReplayStall]       - preData[cpuId].nStall[ReplayStall]; 
    long long diffPortConflictStall = thisData[cpuId].nStall[PortConflictStall] - preData[cpuId].nStall[PortConflictStall]; 
    long long diffSwitchStall       = thisData[cpuId].nStall[SwitchStall]       - preData[cpuId].nStall[SwitchStall]; 
    long long diffStall             = diffWinStall + diffROBStall + diffREGStall + diffLoadStall + diffStoreStall + diffBranchStall + diffPortConflictStall + diffSwitchStall;
    //print to result file
    fprintf(fp,"%d %lf %lf %lf %lf %lf %lf ",cpuId,(double)diffdl1miss/(double)interval,(double)diffil1miss/(double)interval,(double)diffl2miss/(double)interval,(double)diffdl1hit/(double)interval,(double)diffil1hit/(double)interval,(double)diffl2hit/(double)interval);
    
    fprintf(fp," %lf %lf %lf %lf %lf %lf ",(double)diffitlb/(double)interval,(double)diffdtlb/(double)interval,(double)diffStall/(double)interval,(double)diffNBranchHit/(double)interval,(double)diffNBranchMiss/(double)interval,(double)diffNBranchMiss/(double)(diffNBranchHit + diffNBranchMiss));

    fprintf(fp," %lld %lf %lf %lf %lf %lf %lf ",diffclock,diffIPC,L1MissRate,L2MissRate,diffenergy,diffpower,diffIPC/diffpower);
  
    statInst[threadId] = statInst[threadId] + diffinst;

   return statInst[threadId] > totalNInst;
}

int xuStats::phaseClassify(int cpuId){

    int       threadId   = arc_sigma[cpuId];

    long long diffiALU   = thisData[cpuId].nInst[iALU]   - preData[cpuId].nInst[iALU]; 
    long long diffiMult  = thisData[cpuId].nInst[iMult]  - preData[cpuId].nInst[iMult];  
    long long diffiDiv   = thisData[cpuId].nInst[iDiv]   - preData[cpuId].nInst[iDiv]; 
    long long diffiBJ    = thisData[cpuId].nInst[iBJ]    - preData[cpuId].nInst[iBJ]; 
    long long diffiLoad  = thisData[cpuId].nInst[iLoad]  - preData[cpuId].nInst[iLoad]; 
    long long diffiStore = thisData[cpuId].nInst[iStore] - preData[cpuId].nInst[iStore]; 
    long long difffpALU  = thisData[cpuId].nInst[fpALU]  - preData[cpuId].nInst[fpALU]; 
    long long difffpMult = thisData[cpuId].nInst[fpMult] - preData[cpuId].nInst[fpMult]; 
    long long difffpDiv  = thisData[cpuId].nInst[fpDiv]  - preData[cpuId].nInst[fpDiv]; 
    long long INT = diffiALU + diffiMult + diffiDiv;
    long long BJ  = diffiBJ;
    long long FP  = difffpALU + difffpMult + difffpDiv;
    long long MEM = diffiStore + diffiLoad;
    long long diffINT;
    long long diffBJ;
    long long diffFP;
    long long diffMEM;

    double    delt;
    int       tempid    = control_table[threadId][max_ID] + 1;
    int       currentid = control_table[threadId][current_ID];
   
   
if(  !is_Done[threadId] ){  //threadId must be in a core.

	    if(0 == tempid){ // for the first phase
		if(pre[threadId] == false ){
			phase_Z[threadId][0] = INT;
			phase_Z[threadId][1] = BJ;
			phase_Z[threadId][2] = FP;
			phase_Z[threadId][3] = MEM;
			pre[threadId] = true;
		}
		else{
			diffINT = abs(phase_Z[threadId][0] - INT); 
			diffBJ  = abs(phase_Z[threadId][1] - BJ); 
			diffFP  = abs(phase_Z[threadId][2] - FP);
			diffMEM = abs(phase_Z[threadId][3] - MEM);
			delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
			if(delt > ITV_diff){
				control_table[threadId][temp_NUM] = 0;
				phase_Z[threadId][0] = INT;
				phase_Z[threadId][1] = BJ;
				phase_Z[threadId][2] = FP;
				phase_Z[threadId][3] = MEM;
			}
			else {
				if (++control_table[threadId][temp_NUM] >= 4){	
				        nBegin ++;	
					control_table[threadId][temp_NUM] = 0;
					control_table[threadId][max_ID] = tempid;
					control_table[threadId][current_ID] = tempid;
					phase_table[threadId][tempid][0] = phase_Z[threadId][0];
					phase_table[threadId][tempid][1] = phase_Z[threadId][1];
					phase_table[threadId][tempid][2] = phase_Z[threadId][2];
					phase_table[threadId][tempid][3] = phase_Z[threadId][3];
				}
			}
		}
	    }
	    else{ //other phase
			diffINT = abs(phase_table[threadId][currentid][0] - INT); 
			diffBJ  = abs(phase_table[threadId][currentid][1] - BJ); 
			diffFP  = abs(phase_table[threadId][currentid][2] - FP); 
			diffMEM = abs(phase_table[threadId][currentid][3] - MEM); 
			delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
		if( delt >  ITV_diff){                               //may be a new phase  // may a previous phase
			if(++control_table[threadId][temp_NUM] >= 4){// is another phase
				control_table[threadId][temp_NUM] = 0;
				for(int i = 0; i <= control_table[threadId][max_ID]; i++){ //Is previous phase?
					diffINT = abs(phase_table[threadId][i][0] - INT); 
					diffBJ  = abs(phase_table[threadId][i][1] - BJ); 
					diffFP  = abs(phase_table[threadId][i][2] - FP); 
					diffMEM = abs(phase_table[threadId][i][3] - MEM); 
					delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
	
					if(delt <= ITV_diff ){  //a previous phase.
						control_table[threadId][max_ID]--;  //in order to ++
						tempid = i;
						break;
					}
				}
				control_table[threadId][max_ID]++;
				control_table[threadId][current_ID] = tempid; //update current_ID
				phase_table[threadId][tempid][0] = INT;
				phase_table[threadId][tempid][1] = BJ;
				phase_table[threadId][tempid][2] = FP;
				phase_table[threadId][tempid][3] = MEM;
			}
		}
		else{ //is current
			control_table[threadId][temp_NUM] = 0;
		}
	    }
	
	}//end   if( !is_Done[threadId])

       fprintf(fp,"    %lf %lf %lf %lf %lld\n",(double)INT/(double)interval,(double)BJ/(double)interval,(double)FP/(double)interval,(double)MEM/(double)interval,control_table[threadId][current_ID]);
	return currentid;
//
}


void xuStats::solveMigrate(int threadId){
	int cpuId   = sigma_curr[threadId];
	int tempid  = control_table[threadId][current_ID];
	double  INT = phase_table[threadId][tempid][0];
	double  BJ  = phase_table[threadId][tempid][1];
	double  FP  = phase_table[threadId][tempid][2];
	double  MEM = phase_table[threadId][tempid][3];
	
	long long diffclock       = thisData[cpuId].clock - preData[cpuId].clock;
        long long diffinst        = thisData[cpuId].sumInst - preData[cpuId].sumInst;
        double    diffIPC         = (double)diffinst / (double)diffclock;

        long long diffNBranchMiss = thisData[cpuId].nbranchMiss - preData[cpuId].nbranchMiss;
	double C = diffIPC;
	double T = (double)INT/(double)interval;
	double B = (double)diffNBranchMiss/(double)interval;
	double M = (double)MEM/(double)interval;
	double S;
	double L;
	if(cpuId < 5){   //S core
		 S = 0.0407 + 0.1227*C - 0.0013 * T + 0.0110 * B - 0.0371 *M; //SS
		 L = 0.0631 + 0.0162*C + 0.0021 * T - 0.1284 * B - 0.0394 *M;  //SL
	}
	else{  //  Lcore
	
		 L = 0.0623 + 0.0040 * C - 0.0025 * T - 0.0905 * B - 0.0363 *M;  //LL
		 S = 0.0677 + 0.0012 * C - 0.0110 * T + 0.0028 * B - 0.0434 *M;  //LS
	}
        for(int i = 1; i <= 4; i++){
		phase_pw[threadId][tempid][i] = S;
		phase_pw[threadId][tempid][i + 4] = L;
	}
       if(nBegin >= 8 && true == is_Migrate(sigma_curr,sigma_dst)){
	        printf("migrate happened\n");
       		doMigrate(sigma_curr,sigma_dst);
       }	
} 


void xuStats::recoverData(int cpuId){

    preData[cpuId].dl1miss     =  thisData[cpuId].dl1miss;
    preData[cpuId].il1miss     =  thisData[cpuId].il1miss;
    preData[cpuId].l2miss      =  thisData[cpuId].l2miss;
    preData[cpuId].dl1hit      =  thisData[cpuId].dl1hit;
    preData[cpuId].il1hit      =  thisData[cpuId].il1hit;
    preData[cpuId].l2hit       =  thisData[cpuId].l2hit;
    preData[cpuId].itlb        =  thisData[cpuId].itlb;
    preData[cpuId].dtlb        =  thisData[cpuId].dtlb;
    preData[cpuId].nbranche    =  thisData[cpuId].nbranche;
    preData[cpuId].nbranchMiss =  thisData[cpuId].nbranchMiss;
    preData[cpuId].nbranchHit  =  thisData[cpuId].nbranchHit;
    preData[cpuId].clock       =  thisData[cpuId].clock;
    preData[cpuId].sumInst     =  thisData[cpuId].sumInst;
    preData[cpuId].energy      =  thisData[cpuId].energy;
    
    for(int i = iOpInvalid; i < MaxInstType; i++){
	preData[cpuId].nInst[i] = thisData[cpuId].nInst[i];
    } 
    for(int i = NoStall + 1; i < MaxStall; i++){
	preData[cpuId].nStall[i] = thisData[cpuId].nStall[i];
    } 
}


void xuStats::inputToFile(GProcessor *core){
    int cpuId    = core->getId();
    int threadId = arc_sigma[cpuId];
    int currentid;
    bool is_Killed = false;
    if( !is_Done[threadId] && !is_Free[cpuId]){
	    fp = fopen(fileName,"a+");
	    if(NULL == fp){
	    	printf("Can't open file : %s\n",fileName);
		exit(-1);
	    }
	    is_Killed = solveThisData(core);
	    if(is_Killed){
	    	getTotStats(core);
	    
	    }
	    else{
	    	currentid = phaseClassify(cpuId);
	    	if(currentid !=  control_table[threadId][current_ID])
	    		solveMigrate(threadId);
	    	recoverData(cpuId); 
	   }
	    fclose(fp);
    }
}


void xuStats::getTotStats(GProcessor *core){
      
      int   cpuId       = 0;
      int   threadId    = 0;
      long long clock   = 0;
      double    IPC     = 0.0;
      long long sumInst = 0;
      ProcessId  *proc;
      struct statDataContext currContext;
      clock = core->getClockTicks();
      cpuId = core->getId();
      threadId = arc_sigma[cpuId];
      
      if(!is_Free[cpuId] && clock > 0){

           GStatsCntr **ppnInst = core->xuGetNInst();  //sumInst
           for(int i = iOpInvalid; i < MaxInstType; i++){
	   	sumInst += ppnInst[i]->getValue(); 
	   }
	   IPC = (double)sumInst / (double)clock;  //IPC
           const char * coreName = SescConf->getCharPtr("","cpucore",cpuId); 
           double pPower         = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
           double maxClockEnergy = EnergyMgr::get(coreName,"clockEnergy",cpuId);
           double maxEnergy      = EnergyMgr::get(coreName,"totEnergy");
   	   
	   // More reasonable clock energy. 50% is based on activity, 50% of the clock// distributtion is there all the time
           double clockPower     = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
           double corePower      = pPower + clockPower;      //energy
           double coreEnergy     = EnergyMgr::ptoe(corePower);   //power
   	     
           totFp = fopen(totFileName,"a+");
           if(NULL == totFp){
    	         printf("Can't open file : %s\n",fileName);
	          exit(-1);
            }
      	   
	   currContext.clock   = clock;
      	   currContext.sumInst = sumInst;
      	   currContext.IPC     = IPC;
      	   currContext.power   = corePower;
      	   currContext.energy  = coreEnergy;
     	   currContext.pw      = IPC/corePower;

	   fprintf(totFp,"%d ",threadId);
	   fprintf(totFp,"%lld ",sumInst - context[cpuId].sumInst);
	   fprintf(totFp,"%lld ",clock - context[cpuId].clock);
	   fprintf(totFp,"%lf ",IPC -context[cpuId].IPC);
	   fprintf(totFp,"%lf ",corePower -context[cpuId].power);
	   fprintf(totFp,"%lf ",coreEnergy - context[cpuId].energy);
	   fprintf(totFp,"%lf ",IPC/corePower -context[cpuId].pw);
	   fprintf(totFp,"%d \n",0);
	  // fprintf(totFp,"%lf %lf %lf %lf\n",corePower,coreEnergy,IPC/corePower);
		
	   context[cpuId] = currContext;
	   
	   //migrate out threadId;
	   proc  = ProcessId::getProcessId(threadId);
	   osSim->cpus.switchOut(cpuId, proc); //switch out core
      	    
	   is_Free[cpuId]    = true;
	   is_Done[threadId] = true;
              
	   memset(phase_pw[threadId],0,xu_nPhase * xu_nCPU * sizeof(double)); // empty threadId performance/Watt 
	   fclose(totFp);
       }
}



bool xuStats::is_Migrate(int *curr, int *dst){

     double * matrix[xu_nCPU];
     for(int pid = 1; pid < xu_nThread; pid++){
	int phaseid = control_table[pid][current_ID];  
	matrix[pid] = phase_pw[pid][phaseid];
     }
     double max = 0.0; 
     int A[xu_nThread] = {0,1,2,3,4,5,6,7,8};

     dfs(matrix,dst,A,max,1,xu_nThread); // look for the best match
     
     for(int pid = 1; pid < xu_nThread; pid++){
     	 if(curr[pid] != dst[pid]) return true;   //need to mpidgrate  
     }
     return false;
    // matrix[9][9]:  1~8  1~8 is useful , 0 raw and 0 column is invalid
}



void xuStats::dfs(double **matrix, int *dst,int *A,double &max,int start, int end){

	if(start == end){
		double sum = 0.0; 
		for(int i= 1; i< xu_nThread; i ++){
			sum += matrix[i][A[i]];
		}
		if(sum > max){
			max = sum;
			for(int i = 1; i < xu_nThread; i++){
				dst[i] = A[i];
			}
		}
		return;
	}
	else{
		for(int i = start; i < end; i++){
			swap((A+i),(A+start));
			dfs(matrix,dst,A,max,start + 1, end);
			swap((A+i),(A+start));
		}
	}
}

void xuStats::doMigrate(int *curr, int *dst){     //migrate threads from curr to dst.

	int        cpuId;
	ProcessId  *proc;
	nMigrate++;
	
	for(int pid = 1; pid < xu_nThread; pid++){  //switch out
		if(curr[pid] != dst[pid] && !is_Done[pid]){ //thread i need to be migrated form core_curr[i] to core_dst[i]
			cpuId      = curr[pid];
			proc       = ProcessId::getProcessId(pid);
			osSim->cpus.switchOut(cpuId, proc); //switch out proc  from cpuId
		        outputDataThread(cpuId);             //output cpuid's data 
			is_Free[cpuId] = true;
		}
	}
	for(int pid = 1; pid < xu_nThread ; pid++){  //switch in
		if(curr[pid] != dst[pid]){      //thread i need to be migrated form core_curr[i] to core_dst[i]
			curr[pid] = dst[pid];   // update thread->cpu
			cpuId = curr[pid];
			if( !is_Done[pid]){
				proc       = ProcessId::getProcessId(pid);
				osSim->cpus.switchIn(cpuId, proc);
				is_Free[cpuId] = false;
				saveDataContext(cpuId);
			}
			arc_sigma[cpuId] = pid;  // update cpu -> thread
		}
	}
}

void xuStats::outputDataThread(int cpuId){
	int threadId      = 0;
	long long clock   = 0;
	long long sumInst = 0;
	double IPC        = 0.0;
	GProcessor *core  = osSim->id2GProcessor(cpuId);
	
	clock    = core->getClockTicks();
	threadId = arc_sigma[cpuId];

	GStatsCntr **ppnInst = core->xuGetNInst();  //sumInst
	for(int i = iOpInvalid; i < MaxInstType; i++){
		sumInst += ppnInst[i]->getValue(); 
	}

	IPC = (double)sumInst / (double)clock;  //IPC

	const char * procName = SescConf->getCharPtr("","cpucore",cpuId); 
	double pPower         = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
	double maxClockEnergy = EnergyMgr::get(procName,"clockEnergy",cpuId);
	double maxEnergy      = EnergyMgr::get(procName,"totEnergy");
	// More reasonable clock energy. 50% is based on activity, 50% of the clock// distributtion is there all the time
	double clockPower     = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
	double corePower      = pPower + clockPower;      //energy
	double coreEnergy     = EnergyMgr::ptoe(corePower);   //power
	
	totFp = fopen(totFileName,"a+");
	if(NULL == totFp){
		printf("Can't open file : %s\n",fileName);
		exit(-1);
	}
	
	
	fprintf(totFp,"%d   ",threadId);
	fprintf(totFp,"%lld ",sumInst - context[cpuId].sumInst);
	fprintf(totFp,"%lld ",clock - context[cpuId].clock);
	fprintf(totFp,"%lf ",IPC -context[cpuId].IPC);
	fprintf(totFp,"%lf ",corePower -context[cpuId].power);
	fprintf(totFp,"%lf ",coreEnergy - context[cpuId].energy);
	fprintf(totFp,"%lf ",IPC/corePower -context[cpuId].pw);
	fprintf(totFp,"%d  \n",nMigrate);
	fclose(totFp);

}


void xuStats::saveDataContext(int cpuId){

	long long  clock   = 0;
	long long  sumInst = 0;
	double     IPC     = 0.0;
	GProcessor *core   = osSim->id2GProcessor(cpuId);
	clock = core->getClockTicks();
	GStatsCntr **ppnInst = core->xuGetNInst();  //sumInst
	for(int i = iOpInvalid; i < MaxInstType; i++){
 		sumInst += ppnInst[i]->getValue(); 
	}

	IPC = (double)sumInst / (double)clock;  //IPC

	const char * procName  = SescConf->getCharPtr("","cpucore",cpuId); 
	double pPower          = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
	double maxClockEnergy  = EnergyMgr::get(procName,"clockEnergy",cpuId);
	double maxEnergy       = EnergyMgr::get(procName,"totEnergy");
	// More reasonable clock energy. 50% is based on activity, 50% of the clock// distributtion is there all the time
	double clockPower      = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
	double corePower       = pPower + clockPower;      //energy
	double coreEnergy      = EnergyMgr::ptoe(corePower);   //power
	
	context[cpuId].clock   = clock;
	context[cpuId].sumInst = sumInst;
	context[cpuId].IPC     = IPC;
	context[cpuId].power   = corePower;
	context[cpuId].energy  = coreEnergy;
	context[cpuId].pw      = IPC/corePower;

}




