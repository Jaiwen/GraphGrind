#include <utility>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <algorithm>
#include "IO.h"
#include "graph.h"
#include "graphIO.h"
#include "graphUtils.h"
#include "parseCommandLine.h"
#include "dataGen.h"
#include "parallel.h"
#include "gettime.h"
using namespace benchIO;
using namespace dataGen;
using namespace std;
#if 1
#include "cilkpub/sort.h"
template<typename It, typename Cmp>
void mysort( It begin, It end, Cmp cmp )
{
    cilkpub::cilk_sort( begin, end, cmp );
}
#else
template<typename It, typename Cmp>
void mysort( It begin, It end, Cmp cmp )
{
    std::sort( begin, end, cmp );
}
#endif
template <class intT>
class PairSort
{
  intT n;
public:
   PairSort ( intT n_) : n(n_){ }
   bool operator () ( const std::pair<intT,intT> & i, const std::pair<intT,intT> & j ) 
   {
        return i.second < j.second;
        //return (i.second<j.second||(i.second==j.second&&i.first<j.first));
   }
};
template <class intT>
class LargePairSort
{
  intT n;
public:
   LargePairSort ( intT n_) : n(n_){ }
   bool operator () ( const std::pair<intT,intT> &i, const std::pair<intT,intT> & j ) 
   {
        return (i.second>j.second||(i.second==j.second&&i.first<j.first));
   }
};


int parallel_main(int argc, char* argv[])
{
    commandLine P(argc,argv,"[-s] [-r <start>] [-p <part>] <inFile> <outFile>");
    pair<char*,char*> fnames = P.IOFileNames();
    char* iFile = fnames.first;
    char* oFile = fnames.second;
    intT startpos = P.getOptionLongValue("-r",100);
    intT part= P.getOptionLongValue("-p",384);
    bool isSymmetric = P.getOption("-s");
    graph<intT> G = readGraphFromGalois<intT>(iFile,isSymmetric);
#if 0
        int fnl = strlen(iFile);
        char t_fname[fnl+3];
        strcpy( t_fname, iFile );
        strcpy( &t_fname[fnl], "_t" );
    graph<intT> IG = readGraphFromGalois<intT>(t_fname,isSymmetric);
#endif
    intT n = G.n;
    intT m = G.m;
    intT avg = G.n/part;
    intT extra = G.n%part;
    cerr<<"EDGES: "<<m <<" with vertices "<<n<<endl;
    cerr<<"AVERAGE Vertex: "<<avg <<" with extra vertices "<<extra<<endl;
    vertex<intT>* V = G.V;
    //vertex<intT>* V = IG.V;

    cerr<<fnames.first<<" sorting by in-degree from High to Low....."<<endl;
    timer vebo_reorder;
    vebo_reorder.start();
    timer vebo_sort;
    vebo_sort.start();
    std::pair <intT, intT>* check = new std::pair<intT, intT> [n];
    parallel_for(intT i=0; i<n; i++)
    {
      check[i].first=i;
      check[i].second=V[i].getInDegree();
    }
    cerr<<fnames.first<<" Edges loading....."<<endl;
   
    mysort(&check[0],&check[n],LargePairSort<intT>(n));      
    cerr<<"Sort: "<<vebo_sort.stop()<<endl;
    timer vebo_collect;
    vebo_collect.start();
    intT c=1;
    intT czero=0;
    intT cone=0;
    for(intT i=1; i<n; i++)
    {
       if (check[i].second!=check[i-1].second) c++;
       if (check[i].second==0) czero++;
       if (check[i].second==1) cone++;
    }
    cerr<<"highest: "<<check[0].second<<endl;
    cerr<<"2nd: "<<check[1].second<<endl;
    cerr<<"3rd: "<<check[2].second<<endl;
    cerr<<"4th: "<<check[3].second<<endl;
    cerr<<"5th: "<<check[4].second<<endl;
    cerr<<"zero: "<<czero<<endl;
    cerr<<"one: "<<cone<<endl;
    intT* degreeN = new intT [check[0].second+1];
    //intT* degreePerN = new intT [c-1];
    std::pair <intT, intT>* degreePerN = new std::pair<intT, intT> [c];
    parallel_for (intT i =0; i<check[0].second+1; i++){
        degreeN[i]=0;  //same degree number
    }
    parallel_for (intT i=0; i<c; i++)
    {
       degreePerN[i].first=0;  //same degree number
       degreePerN[i].second=0;  //same degree number
    }
    cerr<<"The amount of same degree: "<<c<<endl;
    intT a =0 ;
    for (intT i =0; i<n; i++){
        if (i==0){ 
           degreeN[check[i].second]= a;
           degreePerN[a].first++;
           degreePerN[a].second=check[i].second;
        }
        else{
          if (check[i].second!=check[i-1].second) 
             a++;
          degreeN[check[i].second]= a;
          if (check[i].second!=0) 
          {
            degreePerN[a].first++;
            degreePerN[a].second=check[i].second;
          }
        }
    }

    cerr<<"Collect :"<<vebo_collect.stop()<<endl;
    cerr<<"Chunck size determine"<<endl;
    std::pair <intT, intT>* inter = new std::pair<intT, intT> [n];
    intT* fNinter = new intT [n];

    bool* relabel = new bool [n];
    parallel_for (intT i =0; i<n; i++) 
        relabel[i]=false;

    intT* size = new intT [part];
    intT* ver_size = new intT [part];
    intT* edges = new intT [part];

    parallel_for (intT i =0; i<part; i++) 
    {
        edges[i]=0;
        ver_size[i]=0;
        size[i]=0;
    }

    intT** perDeg = new intT* [part];
    parallel_for (intT i=0; i<part; i++)
    {
       perDeg[i]= new intT [c];
       parallel_for (intT j=0;j<c;j++)
           perDeg[i][j]=0;
    }
    //Assign vertices into corresponding partition, and calculate
    //chunck size
    cerr<<"Phase 1 assign vertices From High to Low degree"<<endl;
    timer vebo_chunk;
    vebo_chunk.start();
#if 1
    for (intT i =0; i<c; i++)
    {
       intT degreeNow=degreePerN[i].second;
       intT same=degreePerN[i].first;
       for (intT k=0; k<same; )
       {
          intT max_edges=edges[0];
          intT min_edges=edges[0]; 
          intT min_pos=0;
          for (intT j=1; j<part; j++)
          {
           if(edges[j]<min_edges){
               min_edges=edges[j];
               min_pos = j;
           } 
           if(edges[j]>max_edges){
               max_edges=edges[j];
           } 
          }
          intT delta = max_edges-min_edges;
          if(delta>degreeNow) 
          {
            intT remain = (same-k)/part;
            if (remain>1)
            {
              edges[min_pos]= edges[min_pos]+ degreeNow*remain; 
              perDeg[min_pos][degreeN[degreeNow]]=perDeg[min_pos][degreeN[degreeNow]]+remain; 
              ver_size[min_pos]=ver_size[min_pos]+remain;
              k=k+remain;
            }
            else
            {
              edges[min_pos]+=degreeNow; 
              perDeg[min_pos][degreeN[degreeNow]]++; 
              ver_size[min_pos]++;
              k++;
            }
          }
          else{
            edges[min_pos]+=degreeNow; 
            perDeg[min_pos][degreeN[degreeNow]]++; 
            ver_size[min_pos]++;
            k++;
          }
        }
     }
#else 
    for (intT i =0; i<n-czero; i++)
    {
          intT min_edges=edges[0]; 
          intT min_pos=0; 
          for (intT j=1; j<part; j++)
          {
           if(edges[j]<min_edges){
               min_edges=edges[j];
               min_pos = j;
           } 
          }
          edges[min_pos]+=check[i].second; 
          perDeg[min_pos][degreeN[check[i].second]]++; 
          ver_size[min_pos]++;
   }
#endif

   cerr<<"Phase 2: Zero vertex determine"<<endl;
   intT kzero=0;
   intT Dzero=czero/part;
#if 1
   if(extra==0){
        cerr<<"Vertex==partition number"<<endl;
        for (intT i=0;i<part;i++)
        {
           if (ver_size[i]<avg)
           {
              intT dif=avg-ver_size[i];
              ver_size[i]=ver_size[i]+dif;
              perDeg[i][degreeN[0]]=perDeg[i][degreeN[0]]+dif; 
           }    
        }
   }
   else{
      if(Dzero!=0){
        cerr<<"More vertices than partition"<<endl;
        for (intT i=0;i<part;i++)
        {
           if (ver_size[i]<avg)
           {
              intT dif=avg-ver_size[i];
              ver_size[i]=ver_size[i]+dif;
              perDeg[i][degreeN[0]]=perDeg[i][degreeN[0]]+dif; 
           }    
        }
        for(intT i=0; i<extra; i++)
        {
              ver_size[i]++;
              perDeg[i][degreeN[0]]++; 
        }
      }
      else{
        cerr<<"Less zero vertices than partition"<<endl;
        for(intT i=0; i<czero; i++)
        {
           intT min_vertex=ver_size[0];
           intT min_ver_pos=0;
           for (intT k=1; k< part; k++)
           {
               if (ver_size[k]<min_vertex){
                   min_vertex=ver_size[k];
                   min_ver_pos=k;
               }
           }
           ver_size[min_ver_pos]++;
           perDeg[min_ver_pos][degreeN[0]]++; 
        }
      }
    }
#endif
    cerr<<"Chunk: "<<vebo_chunk.stop()<<endl;

    cerr<<"Initial allocation"<<endl;
    size[0]=0;
    for (intT i =1; i<part; i++)
    {
      size[i]=ver_size[i-1]+size[i-1];  
    }
    parallel_for (intT i =0; i<part; i++) 
    {
        ver_size[i]=0;
        edges[i]=0;
    }

    timer vebo_fill;
    vebo_fill.start();
    cerr<<"Filling vertices"<<endl;
    intT real=0;
    intT index=0,pos=0;
    for (intT i =0 ; i<c; i++){
        for (intT j=0; j<part; j++){
           if(perDeg[j][i]!=0){
              for (intT k=0;k<perDeg[j][i];k++){
                  pos=check[real].first;
                  index=size[j]+ver_size[j];
                 if(!relabel[index])
                  {
                    fNinter[index]=pos;
                    relabel[index]=true;
                    ver_size[j]++;
                    edges[j]+=check[real].second;
                    real++; 
                  }
                  if(fNinter[index]==startpos)
                  cerr<<"Trace start vertex, new: "<< index<< " old: "<<pos<<endl;
              }
           }
        }
    }
    cerr<<"Fill: "<<vebo_fill.stop()<<endl;
#if 0
    for (intT i =0 ; i< part; i++)
    {
          cerr<<i<<"th part "<<ver_size[i]<<" "<<edges[i]<<endl;
    }    
   abort();
#endif
    
#if 0
    for (intT i=0; i<n; i++)
        if (!relabel[i]) cerr<<"opps"<<i<<endl; 
    abort();
#endif
    timer vebo_graph;
    vebo_graph.start();
    for (intT i=0; i<part; i++)
        delete [] perDeg[i];
    delete [] perDeg;
    delete [] size;
    delete [] edges;
    delete [] degreePerN;
    delete [] ver_size;
    delete [] relabel;
    cerr<<"Inter array sorting and first trace in first relabling"<<endl;
    parallel_for(intT i=0; i<n; i++)
    {
       inter[i].first=i;
       inter[i].second=fNinter[i];
    }
    mysort(&inter[0],&inter[n],PairSort<intT>(n));      
    cerr<<"First partitioning Graph Relabeling"<<endl;
    
    intT * Nedges = new intT [m];
    vertex<intT> * v = newA(vertex<intT>,n);
    intT X=0;
    for(intT i =0; i<n; i++){
         intT o = X;
         v[i].setOutDegree(V[fNinter[i]].degree);
         v[i].Neighbors=Nedges+o;
         for (intT j=0; j<v[i].degree; j++)
         {
            intT pos = V[fNinter[i]].Neighbors[j];
            v[i].Neighbors[j]= inter[pos].first;
         }
         X = X+V[fNinter[i]].degree;
    }
    delete [] fNinter;
    delete [] inter;
    cerr<<fnames.first<<"Get graph....."<<endl;
    cerr<<"Graph: "<<vebo_graph.stop()<<endl;
    cerr<<"Reorder: "<<vebo_reorder.stop()<<endl;
    graph<intT> WG(v,n,m,Nedges);

    G.del();

    cerr<<fnames.first<<" writing Graph....."<<endl;
    writeGraphToFile(WG,oFile);
    free(v);
    delete [] Nedges;
}
