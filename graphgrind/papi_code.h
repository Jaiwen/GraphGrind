#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer.h>
long long **values;
#include <papi.h>
#define NUM_EVENTS 4
#ifndef THREADS_CACHE
#define THREADS_CACHE 0 
#endif
int native = 0x0;
long long Total_local = 0;
long long Total_br_mis = 0;
long long Total_remote = 0;
long long Total_TLB= 0;
long long Total_in= 0;
int n_threads;
bool init=false;
bool *init_start;
int retval;
int *EventSet;
char local_DRAM[]= "OFFCORE_RESPONSE_0:ANY_REQUEST:LLC_MISS_LOCAL:SNP_NONE:SNP_NOT_NEEDED:SNP_MISS:SNP_NO_FWD:u=0:k=0";
char remote_DRAM[]="OFFCORE_RESPONSE_1:ANY_REQUEST:LLC_MISS_REMOTE:SNP_NONE:SNP_NOT_NEEDED:SNP_MISS:SNP_NO_FWD:u=0:k=0";
char ins_count[]="INSTRUCTION_RETIRED:u=0:k=0";
char BR_MIS[]="MISPREDICTED_BRANCH_RETIRED:u=0:k=0";
char TLB[]="perf::PERF_COUNT_HW_CACHE_DTLB:MISS";
int event_codes[NUM_EVENTS];
#define START 1
#define STOP 2

/*----------------------------Functions related to PAPI event counting------------------------------------*/
void on_all_workers_help( size_t i, size_t n, volatile bool * flags, int stage )
{
    if(n>1)
    {
        if( i < n-1 )
        {
            cilk_spawn on_all_workers_help( i+1, n, flags, stage);
        }
    }
    int id= __cilkrts_get_worker_number();
    
    if(n>1)
    {
        if( i == n-2 )
        {
            for (int j=0; j<n; j++)
                flags[j]=true;
        }
        else
        {
           // printf("started busy waiting\n");
            while( !flags[i]); // busy wait
            //printf("ended busy wait\n");
        }
    }
    if(stage == START)
    {
        if(!init_start[id]){
            init_start[id]=true;
           if(PAPI_create_eventset(&EventSet[id])!= PAPI_OK)
              printf("error creating eventset: id = %d \n", id);
           if (PAPI_add_events(EventSet[id], event_codes, NUM_EVENTS) != PAPI_OK)
            printf("event add error: id %d \n", id);
       }
        if(PAPI_start(EventSet[id])!=PAPI_OK) 	//call papi_start(eventset), call papi_stop(eventset, values)
            printf("start error: id =%d \n", id);

    }

    if (stage==STOP)
    {
        if(PAPI_stop(EventSet[id], values[id])!=PAPI_OK)
            printf("stop error: iter: %lu Thread: %d \n", i, id);
    }

    cilk_sync;
}
/*---------------------------------------------------------------------------*/
void start_on_all_workers()
{
    int n = __cilkrts_get_nworkers();
    n_threads=n;
   volatile bool flags[n];
    for( int i=0; i < n; ++i )
    {
        flags[i] = false;
    }
    if(!init){
       init=true;
       init_start= new bool[n];
       EventSet= new int[n];
       values = new long long*[n];
       for( int i=0; i < n; ++i )
       {
           EventSet[i]= PAPI_NULL;
           values[i] = new long long[NUM_EVENTS];
           init_start[i]=false;
        }
    }
    on_all_workers_help( 0, n, flags, START);
}
/*---------------------------------------------------------------------------*/
void stop_on_all_workers()
{
    int n = __cilkrts_get_nworkers();
    volatile bool flags_stop[n];
    for( int i=0; i < n; ++i )
        flags_stop[i] = false;
    on_all_workers_help( 0, n, flags_stop, STOP );
}
/*---------------------------------------------------------------------------*/
inline void PAPI_start_count()
{
    start_on_all_workers( );
}
/*---------------------------------------------------------------------------*/
inline void PAPI_stop_count()
{
    stop_on_all_workers( );
}

/*---------------------------------------------------------------------------*/

/*-------------------------------------------------MAIN()----------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------*/
inline void PAPI_initial()
{

    /* PAPI library initialisations*/
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT)
    {
        printf("PAPI library init error! \n");
        exit(1);
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
    {
        printf("thread init error\n");
        exit(1);
    }
    /*------------Map PAPI event names to hardware event codes--------*/
    if (PAPI_event_name_to_code(local_DRAM,&event_codes[0]) != PAPI_OK)
    {
        printf("event 1 name error\n");
        exit(1);
    }
    if (PAPI_event_name_to_code(remote_DRAM,&event_codes[1]) != PAPI_OK)
    {
        printf("event 2 name error");
        exit(1);
    }
#if 0
    if (PAPI_event_name_to_code(ins_count,&event_codes[2]) != PAPI_OK)
    {
        printf("event 3 name error");
        exit(1);
    }
#endif
    if (PAPI_event_name_to_code(BR_MIS,&event_codes[2]) != PAPI_OK)
    {
        printf("event 4 name error");
        exit(1);
    }

    if (PAPI_event_name_to_code(TLB,&event_codes[3]) != PAPI_OK)
    {
        printf("event 5 name error");
        exit(1);
    }
    /*-----------------------------------------------------------------*/
}
inline void PAPI_print()
{

    /*-------- print values of PAPI counters for all threads------*/
#if THREADS_CACHE
    printf("Threads L3_Local	L3_REMOTE	BR_RET_MIS	TLB\n");
    for (int k=0; k<n_threads; k++)
    {
    printf("%d\t ", k);   /*L3_MISS*/
    printf("%llu\t", values[k][0]);   /*L3_MISS*/
    printf("%llu\t", values[k][1]);   /*TLB_MISS*/
    printf("%llu\t", values[k][2]);   /*Ins_MISS*/
    printf("%llu\n", values[k][3]);   /*TLB_MISS*/
    }
#endif
//    printf("L3_MISS_Local	L3_MISS_REMOTE	MIS_BRANCH_PRE	TLB\n");
    long long TLB_total=0;
    long long br_mis_total=0;
    long long L3_local=0;
    long long L3_remote=0;
    for (int k=0; k<n_threads; k++)
    {
          L3_local+=values[k][0];
     //     br_exe_total+=values[k][2];
          br_mis_total+=values[k][2];
          L3_remote+=values[k][1];
          TLB_total+=values[k][3];
    }
#if 0
    printf("%llu\t", L3_local);   /*L3_MISS*/
    printf("%llu\t", L3_remote);   /*TLB_MISS*/
    printf("%llu\t", br_mis_total);   /*Ins_MISS*/
    printf("%llu\n", TLB_total);   /*TLB_MISS*/
    printf("\n");
#endif
    for (int k=0; k<n_threads; k++)
    {
          values[k][0]=0;
          values[k][2]=0;
          values[k][3]=0;
          values[k][1]=0;
     //     values[k][4]=0;
	}
    Total_local+=L3_local;
    Total_remote+=L3_remote;
    Total_TLB+=TLB_total;
    Total_br_mis+=br_mis_total;
    //delete [] values;
}

inline void PAPI_total_print(int rounds){
    long long local=Total_local/rounds;
    long long rem = Total_remote/rounds;
    long long tlb = Total_TLB/rounds;
    long long br= Total_br_mis/rounds;
    printf("L3_Total_Local	L3_Total_REMOTE	BR_RET_MIS	TLB\n");
    printf("%llu\t", local);   /*L3_MISS*/
    printf("%llu\t", rem);   /*TLB_MISS*/
    printf("%llu\t", br);   /*TLB_MISS*/
    printf("%llu\n", tlb);   /*TLB_MISS*/
    printf("\n");
    long long Total_local=0;
    long long Total_remote=0;
    long long Total_TLB=0;
    long long Total_br_mis=0;
}
inline void PAPI_end(){
    delete [] init_start;
    delete [] EventSet;
    delete [] values;
}

