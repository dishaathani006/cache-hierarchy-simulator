#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <cmath>
#include <string>
#include <cstdint>
#include <inttypes.h>
#include <sstream> 
#include <algorithm>
#include <iomanip>  // For setw()
#include "sim.h"
using namespace std;
// cache block structure

int L1reads=0;
int L1read_misses=0;
int L1writes=0;
int L1write_misses=0;
int L1wbL2=0;
int L1wb=0;
int L1wbMem=0;
int L1pref=0;

float L1missRate=0;
float L2missRate=0;

int L2reads=0;
int L2read_misses=0;
int L2reads_pref=0;
int L2read_misses_pref=0;
int L2writes=0;
int L2write_misses=0;
int L2wbMem=0;
int L2wb=0;
int L2pref=0;

int TotalMemtraffic=0;

struct CacheBlock {
    bool valid;       // Valid bit
    bool dirty;       // Dirty bit
    uint32_t tag; // Tag
    //uint32_t index;
    int lruCount;   // LRU counter for replacement policy

    CacheBlock() : valid(false), dirty(false), tag(0), lruCount(-1) {}
};

struct StreamBuffer
{
   vector<uint32_t> blocks;
   int head;
   bool valid;
   int lrucount;
   StreamBuffer(int M) : blocks(M,0),head(0), valid(false), lrucount(-1) {}
};

class Cache
{
   public:
   Cache(int Cache_Size, int block_Size, int Assoc, int PREF_N, int PREF_M)
        : Cache_Size(Cache_Size), block_Size(block_Size), Assoc(Assoc), PREF_N(PREF_N),PREF_M(PREF_M) 
        {
         block_offset_bits=log2(block_Size);
         blockOffsetMask=(1 << block_offset_bits) - 1;
         if(Cache_Size!=0 && Assoc!=0)
         {
        sets = Cache_Size / (block_Size * Assoc);
        index_bits=log2(sets);
        blockOffsetMask = (1 << block_offset_bits) - 1;
        indexMask = (1 << index_bits) - 1;
        cache.resize(sets, vector<CacheBlock>(Assoc));
         }
        //MultiBuffers.resize(PREF_N, vector<StreamBuffer>(PREF_M));
        MultiBuffers.resize(PREF_N,StreamBuffer(PREF_M));
    }
    bool IsHit(uint32_t addr, bool IsWrite)
    {
      uint32_t index= getIndex(addr);
      uint32_t tag= getTag(addr);
      for(int i=0;i<Assoc;i++)
      {
         if(cache[index][i].valid && cache[index][i].tag==tag)
         {
            //int x=cache[index][i].lruCount;
            //cache[index][i].lruCount=0;
            if(IsWrite)
            {
               cache[index][i].dirty=IsWrite;
            }
            updateLRU(index,i);
            return true;
         }
      }
      return false;
    }
    void updateLRU(uint32_t index,int LRUBlock)
    {
      for(int i=0;i<Assoc;i++)
      {
         if(cache[index][LRUBlock].lruCount!=-1)
         {
            if(cache[index][i].lruCount<cache[index][LRUBlock].lruCount && cache[index][i].valid)
            {
            cache[index][i].lruCount++;
            //cout<<"lru count:"<<cache[index][i].lruCount<<" Lrublock:"<<LRUBlock<<endl;
            }
         }
         else
         {
            if(i<LRUBlock && cache[index][i].valid)
            {
               cache[index][i].lruCount++;
            }
         }
         
      }
      cache[index][LRUBlock].lruCount=0;
   
      }

    int SearchLRU(uint32_t addr)
    {
      uint32_t index= getIndex(addr);
      uint32_t tag= getTag(addr);
      int max=-1;
      //int k=0;
      int findLRU=0;
      for(int i=0;i<Assoc;i++)
      {
         if(!cache[index][i].valid)
         {
            return i;
            //break;
         }
         else if(max<cache[index][i].lruCount)
         {
            max=cache[index][i].lruCount;
            findLRU=i;
         }
      }
      return findLRU;
    }

    void AddBlock(uint32_t addr,int LRUBlock,bool IsWrite)
    {
      uint32_t index= getIndex(addr);
      uint32_t tag= getTag(addr);
      cache[index][LRUBlock].tag=tag;
      cache[index][LRUBlock].valid=true;
      cache[index][LRUBlock].dirty=IsWrite;

      //int x=cache[index][LRUBlock].lruCount;
      //cache[index][LRUBlock].lruCount=0;
      updateLRU(index,LRUBlock);
      //printf("LRU Block: %d, Is Dirty: %d\n", LRUBlock, cache[index][LRUBlock].dirty);

    }

    bool IsDirty(uint32_t addr, int LRUBlock)
    {
      uint32_t index=getIndex(addr);
      if(cache[index][LRUBlock].dirty)
      {
         return true;
      }
      return false;
    }

    uint32_t writeback(uint32_t addr, int LRUBlock)
    {
      uint32_t index= getIndex(addr);
      uint32_t blockoffset=getblockoffset(addr);
      uint32_t concataddr=getaddr1(cache[index][LRUBlock].tag,index,blockoffset);
      cache[index][LRUBlock].dirty=0;
      /*ostringstream oss;
      oss<<hex<<cache[index][LRUBlock].tag<<hex<<index<<hex<<blockoffset;
      string result=oss.str();
      uint32_t concatAddr= stoul(result,nullptr,16);
      cout<<concatAddr<<" ";*/
      //char result[32];
      //sprintf(result,"%x%x",cache[index][LRUBlock],index);
      return concataddr;
      //printf("LRU Block: %d, Is Dirty: %d\n", LRUBlock, cache[index][LRUBlock].dirty);
    }

    int SetDirty(uint32_t addr)
    {
      uint32_t tag=getTag(addr);
      uint32_t index= getIndex(addr);
      //cout<<"addr while setting it dirty:"<<addr<<endl;
      int L2WriteHit=0;
      //ostringstream oss;
      for(int i=0;i<Assoc;i++)
      {
         /*oss<<hex<<cache[index][i].tag<<index<<block_offset_bits;
         string result=oss.str(); // tag and index concatenate
         cout<<result<<endl;*/
         if(cache[index][i].tag==tag && cache[index][i].valid)
         {
            cache[index][i].dirty=true;
            L2WriteHit=1;
            updateLRU(index,i);
            return L2WriteHit;
            //cout<<"set: "<<index<<" "<<"tag: "<<LRUtag<<" "<<"Dirty: "<<cache[index][i].dirty<<endl;
         }
         
      }
      return L2WriteHit;
    }

   bool prefetchHit(uint32_t addr)
   {
    uint32_t tag=getTag(addr);
    uint32_t index= getIndex(addr);
    uint32_t blockaddr=getblock(tag,index);
    int k=0;
    int findmru=0;
    int hit=0;
    int max=PREF_N;
    for(int i=0;i<PREF_N;i++)
    {
        if(MultiBuffers[i].valid && max>MultiBuffers[i].lrucount)
        {
            for(int j=0;j<PREF_M;j++)
            {
                if(blockaddr==MultiBuffers[i].blocks[j])
                {
                    max=MultiBuffers[i].lrucount;
                    k=1;
                    findmru=i;
                    hit=j;
                    /*if(j+1>=PREF_M)
                    {
                        MultiBuffers[i].head=0;
                    }
                    else
                    {MultiBuffers[i].head=j+1;}*/
                    //MultiBuffers[i].lrucount=0;
                    //updateBufferLRU(i);
                    //return true;
                }
            }
        }
    }
    
    //updateBufferLRU(findmru);
    if(k==1)
    {
         if(hit==PREF_M-1)
         {
            MultiBuffers[findmru].head=0;
         }
         else
         {
            MultiBuffers[findmru].head=hit+1;
         }
        updateBufferLRU(findmru);
        return true;
    }
    return false;
   }

   void updateBufferLRU(uint32_t buffer)
   {
        for(int i=0;i<PREF_N;i++)
        {
            if(MultiBuffers[buffer].lrucount!=-1)
            {
                if(MultiBuffers[i].lrucount<MultiBuffers[buffer].lrucount && MultiBuffers[i].valid)
                {
                    MultiBuffers[i].lrucount++;
                }
            }
            else
            {
                if(i<buffer && MultiBuffers[i].valid)
                {
                    MultiBuffers[i].lrucount++;
                }
            }
        }
        MultiBuffers[buffer].lrucount=0;
   }

   uint32_t SearchBufferLRU(uint32_t addr)
   {
    uint32_t tag=getTag(addr);
    uint32_t index=getIndex(addr);
    uint32_t blockaddr=getblock(tag, index);
    int max=-1;
    int findlru;
    for(int i=0;i<PREF_N;i++)
    {
        if(!MultiBuffers[i].valid)
        {
            updateBufferLRU(i);
            return i;
        }
        else if(max<MultiBuffers[i].lrucount)
        {
            max= MultiBuffers[i].lrucount;
            findlru=i;
        }
    }
    updateBufferLRU(findlru);
    return findlru;
   } 

   uint32_t addStreams(uint32_t addr)
   {
    uint32_t tag=getTag(addr);
    uint32_t index= getIndex(addr);
    uint32_t blockaddr=getblock(tag, index);
    //cout<<"addr:"<<hex<<addr<<" blockaddr:"<<hex<<blockaddr<<endl;
    uint32_t prefetches=0;
    for(int i=0;i<PREF_N;i++)
    {
        if(MultiBuffers[i].lrucount==0)
        {
            if(MultiBuffers[i].head==0)
            {
                prefetches=PREF_M;
            }
            else
            {
                prefetches=((MultiBuffers[i].head))%PREF_M;
            }
            MultiBuffers[i].valid=true;
            MultiBuffers[i].head=0;
            for(int j=0;j<PREF_M;j++)
            {
                MultiBuffers[i].blocks[j]=blockaddr+j+1;
                //cout<<MultiBuffers[i].blocks[j]<<endl;
                //cout<<tag<<" "<<MultiBuffers[i].blocks[j]<<endl;
                //cout<<PREF_N<<" "<<PREF_M;
            }

        }
    }
    return prefetches;
   }

void displaycontents()
 {
    string s;
    bool setHasValidBlock = false; // Flag to track if the set has any valid blocks
    //cout << "===== L1 contents =====" << endl;

    for (int i = 0; i < sets; i++)
    {
        setHasValidBlock = false;
        vector<CacheBlock> sortedBlocks = cache[i];

        // Sort by lruCount in ascending order to get MRU first
        sort(sortedBlocks.begin(), sortedBlocks.end(), [](const CacheBlock &a, const CacheBlock &b) {
            return a.lruCount < b.lruCount;
        });

        // Prepare a temporary string to store the formatted line for the set
        stringstream line;

        // Iterate through the sorted blocks and display valid blocks
        for (const auto &block : sortedBlocks)
        {
            if (block.valid) // Check if the block is valid
            {
                if (!setHasValidBlock) // First time a valid block is encountered
                {
                    line <<setw(4)<< "set\t" <<setw(5)<<dec<<i << ":   "; // Start the line with the set number
                    setHasValidBlock = true;
                }

                // Check if the block is dirty and set the appropriate flag
                if (block.dirty)
                {
                    s = " D ";
                }
                else
                {
                    s = "   ";
                }

                // Append the block's tag and dirty flag to the line
                line <<setw(8)<< hex << block.tag << s;
            }
        }

        // If the set had at least one valid block, print the formatted line
        if (setHasValidBlock)
        {
            cout << line.str() << endl; // Print the contents of the set
        }
    }
    cout<<dec;
  }

  void displayStreamsContents()
  {

   /*for(int i=0;i<PREF_N;i++)
   {
      cout<<i<<" lrucount:"<<MultiBuffers[i].lrucount;
      cout<<endl;
   }
   cout<<endl;*/
   sort(MultiBuffers.begin(), MultiBuffers.end(), [](const StreamBuffer &a, const StreamBuffer &b) {
        return a.lrucount < b.lrucount;  // Sort by lrucount
    });
    //cout<<"Pref N:"<<PREF_N<<" Pref M:"<<PREF_M;
   for(int i=0;i<PREF_N;i++)
   {
      if(MultiBuffers[i].valid)
      {
         for(int j=0;j<PREF_M;j++)
         {
            //MultiBuffers[1].blocks[2]=62;
            
               cout<<setw(8)<<hex<<MultiBuffers[i].blocks[j]<<"  ";
            
         }
         cout<<endl;
         //cout<<i;
      }
      //cout<<endl;
   }
   cout<<endl;
   cout<<dec;
  }

    bool IsWrite(char rw)
    {
      if(rw=='w')
      {
         return true;
      }
      return false;
    }
   public:
   uint32_t Cache_Size;
   uint32_t block_Size;
   uint32_t block_offset_bits;
   uint32_t sets;
   uint32_t Assoc;
   //uint32_t block_offset_bits;
   uint32_t index_bits;
   uint32_t blockOffsetMask;
   uint32_t indexMask;
   uint32_t PREF_M;
   uint32_t PREF_N;

   vector<vector<CacheBlock>> cache;
   //vector<vector<StreamBuffer>> MultiBuffers;
   vector<StreamBuffer> MultiBuffers;
   int currentLRU = 0; // Global LRU counter

   unsigned int getIndex(uint32_t addr)
   {
      return (addr >> block_offset_bits) & indexMask;

   }

   unsigned int getTag(uint32_t addr)
   {
      return addr >> (block_offset_bits + index_bits);
   }

   unsigned int getblockoffset(uint32_t addr)
   {
      return addr & blockOffsetMask;
   }

   unsigned int getaddr1(uint32_t tag, uint32_t index, uint32_t blockoffset)
   {
      return (tag << (index_bits + block_offset_bits)) | (index << block_offset_bits) | blockoffset;
   }

   unsigned int getblock(uint32_t tag, uint32_t index)
   {
      return (tag << index_bits) | index;
   }
};
class CacheSim
{
   public:
   CacheSim(int L1Size, int L1BlockSize, int L1_Assoc,
                   int L2Size, int L2BlockSize, int L2_Assoc,
                   int Pref_N, int Pref_M)
        : L1Cache(L1Size, L1BlockSize, L1_Assoc,Pref_N,Pref_M),
          L2Cache(L2Size, L2BlockSize, L2_Assoc,Pref_N,Pref_M) {}
   int hit=0;
   int miss=0; 
   int x=0;  
   //int L1_LRUBlock=0;  
   //int L2_LRUBlock=0;  
   void Check(char rw, uint32_t addr)
   {
      //L1Cache.L1Int(rw,addr);
      //L1Cache.cache
      //int LRU=0;
   // L2 Cache Logic
   if(L2Cache.Cache_Size!=0)
   {
      if(!L1Cache.IsWrite(rw))
      { // rw = read
         L1reads++;
         if(L1Cache.IsHit(addr,false))
         { // L1 rd Hit
            hit++;
         }
         else 
         { //L1 read Miss
            L1read_misses++;
            L2reads++;
            // Returns the index of the next available block or the block that needs to be evicted
            int L1_LRUBlock=L1Cache.SearchLRU(addr);
            //cout<<"L1 lru block after L1 read miss:"<<L1_LRUBlock<<endl;
            // 
            if(L1Cache.IsDirty(addr,L1_LRUBlock))
            {
               //cout<<"L1 wb to L2:"<<endl;
               uint32_t LRUtag1=L1Cache.writeback(addr,L1_LRUBlock);//L1 writeback to L2
               //cout<<LRUtag1<<endl;
               L1wbL2++;
               L1wb++;
               L2writes++;
               //cout<<std::hex<<LRUtag1<<endl<<"L2 set bit dirty"<<endl;
               int L2writehit=L2Cache.SetDirty(LRUtag1);
               if(L2writehit==0)
               {
                    //L2write_misses++;
                    if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                            if(L2Cache.prefetchHit(LRUtag1))
                            {
                                    //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                                    uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                                    L2pref=L2pref+prefetches;
                            }
                            else
                            {
                                    L2write_misses++;
                                    uint32_t getlru=L2Cache.SearchBufferLRU(LRUtag1);
                                    uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                                    L2pref=L2pref+prefetches;
                            }
                    }
                    else
                    {
                        L2write_misses++;
                    }

                    int L2_LRUBlock=L2Cache.SearchLRU(LRUtag1);
                  //cout<<"L2 lru block after L2 read miss:"<<L2_LRUBlock<<endl;
                    if(L2Cache.IsDirty(LRUtag1,L2_LRUBlock))
                    {
                        //cout<<"L2 wb to mem:"<<endl;
                        uint32_t LRUtag2=L2Cache.writeback(LRUtag1,L2_LRUBlock); //L2 writeback to main memory
                        L2wbMem++;
                        L2wb++;
                        TotalMemtraffic++;
                        //L2Cache.SetDirty(addr,LRUtag);
                    }

                    L2Cache.AddBlock(LRUtag1,L2_LRUBlock,true);
               }
               else 
               {
                  if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                        if(L2Cache.prefetchHit(LRUtag1))
                        {
                            //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                            uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                            L2pref=L2pref+prefetches;
                        }
                    }
               }
            }

               if(L2Cache.IsHit(addr,false))
               {
                    hit++;
                    L1Cache.AddBlock(addr,L1_LRUBlock,false);
                    if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                        if(L2Cache.prefetchHit(addr))
                        {
                            //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                            uint32_t prefetches=L2Cache.addStreams(addr);
                            L2pref=L2pref+prefetches;
                        }
                    }
               }
               else 
               {
                  //L2read_misses++;

                    if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                            if(L2Cache.prefetchHit(addr))
                            {
                                    //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                                    uint32_t prefetches=L2Cache.addStreams(addr);
                                    L2pref=L2pref+prefetches;
                            }
                            else
                            {
                                L2read_misses++;
                                    uint32_t getlru=L2Cache.SearchBufferLRU(addr);
                                    uint32_t prefetches=L2Cache.addStreams(addr);
                                    L2pref=L2pref+prefetches;
                            }
                    }
                    else
                    {
                        L2read_misses++;
                    }   

                  int L2_LRUBlock=L2Cache.SearchLRU(addr);
                  //cout<<"L2 lru block after L2 read miss:"<<L2_LRUBlock<<endl;
                  if(L2Cache.IsDirty(addr,L2_LRUBlock))
                  {
                     //cout<<"L2 wb to mem:"<<endl;
                     uint32_t LRUtag2=L2Cache.writeback(addr,L2_LRUBlock); //L2 writeback to main memory
                     L2wbMem++;
                     L2wb++;
                     TotalMemtraffic++;
                     //L2Cache.SetDirty(addr,LRUtag);
                  }
            
                     TotalMemtraffic++;
                     //cout<<"L2:"<<endl;
                     L2Cache.AddBlock(addr,L2_LRUBlock,false);
                     //L2Cache.UpdateCache(addr);
                     //cout<<"L1:"<<endl;
                     L1Cache.AddBlock(addr,L1_LRUBlock,false);
                     //L1Cache.UpdateCache(addr);
                  
               }
            
         }
         /*uint32_t tag= L1Cache.getTag(addr);
         uint32_t index=L1Cache.getIndex(addr);
         if(L1Cache.cache[index][])*/
      }
      else 
      {
         L1writes++;
         if(L1Cache.IsHit(addr,true))
         {
            hit++;
            //L1Cache.UpdateCache(addr);
         }
         else 
         {
            L1write_misses++;
            L2reads++;
            int L1_LRUBlock=L1Cache.SearchLRU(addr);
            //cout<<"L1 lru block after L1 write miss:"<<L1_LRUBlock<<endl;
            if(L1Cache.IsDirty(addr,L1_LRUBlock))
            {
               //cout<<"L1 lru block is dirty and wb to L2:"<<endl;
               uint32_t LRUtag1=L1Cache.writeback(addr,L1_LRUBlock);//L1 writeback to L2
               L1wbL2++;
               L1wb++;
               L2writes++;
               int L2writehit=L2Cache.SetDirty(LRUtag1);
               //cout<<"LRU tag for L1 wb:"<<LRUtag1<<" if tag was in L2:"<<L2writehit;
               if(L2writehit==0)
               {
                    //L2write_misses++;
                    if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                        if(L2Cache.prefetchHit(LRUtag1))
                        {
                                //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                                uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                                L2pref=L2pref+prefetches;
                        }
                        else
                        {
                            L2write_misses++;
                                uint32_t getlru=L2Cache.SearchBufferLRU(LRUtag1);
                                uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                                L2pref=L2pref+prefetches;
                        }
                    }
                    else
                    {
                        L2write_misses++;
                    }

                    int L2_LRUBlock=L2Cache.SearchLRU(LRUtag1);
                  //cout<<"L2 lru block after L2 read miss:"<<L2_LRUBlock<<endl;
                  if(L2Cache.IsDirty(LRUtag1,L2_LRUBlock))
                  {
                     //cout<<"L2 is dirty after l2 read miss:"<<endl;
                     uint32_t LRUtag2=L2Cache.writeback(LRUtag1,L2_LRUBlock); //L2 writeback to main memory
                     //cout<<LRUtag1<<endl;
                     L2wbMem++;
                     L2wb++;
                     TotalMemtraffic++;
                     //L2Cache.SetDirty(addr,LRUtag);
                  }

                  L2Cache.AddBlock(LRUtag1,L2_LRUBlock,true);
               }
               else
               {
                    if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                    {
                    if(L2Cache.prefetchHit(LRUtag1))
                    {
                        //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L2Cache.addStreams(LRUtag1);
                        L2pref=L2pref+prefetches;
                    }
                    }
               }
            }
            
               if(L2Cache.IsHit(addr,false))
               {
                hit++;
                L1Cache.AddBlock(addr,L1_LRUBlock,true);
                if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                {
                    if(L2Cache.prefetchHit(addr))
                    {
                        //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L2Cache.addStreams(addr);
                        L2pref=L2pref+prefetches;
                    }
                }
               //L2Cache.UpdateCache(addr);
               }
               else 
               {
                  //L2read_misses++;

                  if(L2Cache.PREF_M!=0 && L2Cache.PREF_N!=0)
                  {
                        if(L2Cache.prefetchHit(addr))
                        {
                                //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                                uint32_t prefetches=L2Cache.addStreams(addr);
                                L2pref=L2pref+prefetches;
                        }
                        else
                        {
                              L2read_misses++;
                                uint32_t getlru=L2Cache.SearchBufferLRU(addr);
                                uint32_t prefetches=L2Cache.addStreams(addr);
                                L2pref=L2pref+prefetches;
                        }
                  }
                  else
                  {
                    L2read_misses++;
                  }

                  int L2_LRUBlock=L2Cache.SearchLRU(addr);
                  //cout<<"L2 lru block after L2 read miss:"<<L2_LRUBlock<<endl;
                  if(L2Cache.IsDirty(addr,L2_LRUBlock))
                  {
                     //cout<<"L2 is dirty after l2 read miss:"<<endl;
                     uint32_t LRUtag2=L2Cache.writeback(addr,L2_LRUBlock); //L2 writeback to main memory
                     L2wbMem++;
                     L2wb++;
                     TotalMemtraffic++;
                     //L2Cache.SetDirty(addr,LRUtag);
                  }

                     TotalMemtraffic++;
                     L2Cache.AddBlock(addr,L2_LRUBlock,false);
                     //L2Cache.UpdateCache(addr);
                     L1Cache.AddBlock(addr,L1_LRUBlock,true);
                     //L1Cache.UpdateCache(addr);
                  
               }
            
         
         }
      }
   }
   else
      {
         if(!L1Cache.IsWrite(rw))
         {
            L1reads++;
            if(L1Cache.IsHit(addr,false))
            {
               //hit++;
               if(L1Cache.PREF_M!=0 && L1Cache.PREF_N!=0)
               {
                if(L1Cache.prefetchHit(addr))
                {
                    //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                    uint32_t prefetches=L1Cache.addStreams(addr);
                    L1pref=L1pref+prefetches;
                }
               }
               
            }
            else
            {
                
               //L1read_misses++;
               if(L1Cache.PREF_M!=0 && L1Cache.PREF_N!=0)
               {
                if(L1Cache.prefetchHit(addr))
                {
                        //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L1Cache.addStreams(addr);
                        L1pref=L1pref+prefetches;
                }
                else
                {
                    L1read_misses++;
                        uint32_t getlru=L1Cache.SearchBufferLRU(addr);
                        //cout<<getlru<<endl;
                        uint32_t prefetches=L1Cache.addStreams(addr);
                        L1pref=L1pref+prefetches;
                }
               }
               else 
               {
                L1read_misses++;
               } 
               int L1_LRUBlock=L1Cache.SearchLRU(addr);
               if(L1Cache.IsDirty(addr,L1_LRUBlock))
               {
                  //cout<<"L1 lru block is dirty and wb to L2:"<<endl;
                  uint32_t LRUtag1=L1Cache.writeback(addr,L1_LRUBlock);//L1 writeback to L2
                  L1wbMem++;
                  L1wb++;
                  TotalMemtraffic++;
               }
               TotalMemtraffic++;
               L1Cache.AddBlock(addr,L1_LRUBlock,false);
            }
         }
         else
         {
            L1writes++;
            if(L1Cache.IsHit(addr,true))
            {
               hit++;
               if(L1Cache.PREF_M!=0 && L1Cache.PREF_N!=0)
               {
                if(L1Cache.prefetchHit(addr))
                {
                        //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L1Cache.addStreams(addr);
                        L1pref=L1pref+prefetches;
                }
               }
               //L1Cache.UpdateCache(addr);
               //cout<<hit;
            }
            else
            {
               //L1write_misses++;
               if(L1Cache.PREF_M!=0 && L1Cache.PREF_N!=0)
               {
                if(L1Cache.prefetchHit(addr))
                {
                        //uint32_t getlru= L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L1Cache.addStreams(addr);
                        L1pref=L1pref+prefetches;
                }
                else
                {
                        L1write_misses++;
                        uint32_t getlru=L1Cache.SearchBufferLRU(addr);
                        uint32_t prefetches=L1Cache.addStreams(addr);
                        L1pref=L1pref+prefetches;
                }
               }
               else 
               {
                    L1write_misses++;
               }
               int L1_LRUBlock=L1Cache.SearchLRU(addr);
               if(L1Cache.IsDirty(addr,L1_LRUBlock))
               {
                  //cout<<"L1 lru block is dirty and wb to L2:"<<endl;
                  uint32_t LRUtag1=L1Cache.writeback(addr,L1_LRUBlock);//L1 writeback to L2
                  L1wbMem++;
                  L1wb++;
                  TotalMemtraffic++;
               }
               TotalMemtraffic++;
               L1Cache.AddBlock(addr,L1_LRUBlock,true);
            }
         }
         
      }
   }

   void display()
   {
    if(L2Cache.Cache_Size!=0)
    {
    cout<<"===== L1 contents ====="<<endl;
    L1Cache.displaycontents();
    cout<<endl;
    cout<<"===== L2 contents ====="<<endl;
    L2Cache.displaycontents();
    cout<<endl;
    }
    else
    {
        cout<<"===== L1 contents ====="<<endl;
        L1Cache.displaycontents();
        cout<<endl;
    }
   }

   void displaystreams()
   {
      if(L2Cache.Cache_Size!=0)
    {
      cout<<"===== Stream Buffer(s) contents ====="<<endl;
      L2Cache.displayStreamsContents();
    }
    else
    {
        cout<<"===== Stream Buffer(s) contents ====="<<endl;
        L1Cache.displayStreamsContents();
    }
   }
   public:
      //vector<StreamBuffer> MultiBuffers;
      //Cache MultiBuffers;
      Cache L1Cache;
      Cache L2Cache;
};
/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main (int argc, char *argv[]) {
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.
   /*block_bitsL1 bL1;
   block_bitsL2 bL2;
   cache_L1 blockL1;
   cache_L2 blockL2;  */      

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   //parsing of bits
   /*bL1.block_offset_bits=log2(params.BLOCKSIZE);
   bL1.sets=params.L1_SIZE/(params.L1_ASSOC*params.BLOCKSIZE);
   bL1.index_bits=log2(bL1.sets);
   bL1.tag_bits=32-bL1.index_bits-bL1.block_offset_bits;

   bL2.block_offset_bits=log2(params.BLOCKSIZE);
   bL2.sets=params.L2_SIZE/(params.L2_ASSOC*params.BLOCKSIZE);
   bL2.index_bits=log2(bL1.sets);
   bL2.tag_bits=32-bL2.index_bits-bL2.block_offset_bits;*/

   CacheSim cacheHierarchy(params.L1_SIZE, params.BLOCKSIZE, params.L1_ASSOC,
                                  params.L2_SIZE, params.BLOCKSIZE, params.L2_ASSOC,
                                  params.PREF_N,params.PREF_M);

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      /*if (rw == 'r')
         printf("r %x\n", addr);
      else if (rw == 'w')
         printf("w %x\n", addr);
      else {
         printf("Error: Unknown request type %c.\n", rw);
	 exit(EXIT_FAILURE);
      }*/
      cacheHierarchy.Check(rw,addr);
      //cacheHierarchy.display();
   }
      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      
      //cacheHierarchy.display(addr);
      ///////////////////////////////////////////////////////
    //cout<<"Pref N: "<<params.PREF_N<<" Pref M:"<<params.PREF_M<<endl;
    cacheHierarchy.display();
    if(params.PREF_N!=0 && params.PREF_M!=0)
    {
      cacheHierarchy.displaystreams();
    }

    if(params.L2_SIZE!=0 && params.L2_ASSOC!=0){
    L2reads=L1read_misses+L1write_misses;}
    else
    {
      L2reads=0;
    }

   if((L1reads+L1writes)!=0)
   {
    L1missRate=static_cast<float>(L1read_misses+L1write_misses)/(L1reads+L1writes);
   }
    if(L2reads!=0)
    {
    L2missRate=static_cast<float>(L2read_misses)/L2reads;
    }

    if(params.L2_SIZE!=0 && params.L2_ASSOC!=0)
    {
      TotalMemtraffic=L2read_misses+L2write_misses+L2wb+L2pref;
    }
    else
    {
      TotalMemtraffic=L1read_misses+L1write_misses+L1wb+L1pref;
    }

    cout<< "===== Measurements =====" << endl;

   // Set width for alignment
    cout << left; // Align the labels to the left
    cout << setw(30) << "a. L1 reads:" << left << setw(10) << L1reads << endl;
    cout << setw(30) << "b. L1 read misses:" << left << setw(10) << L1read_misses << endl;
    cout << setw(30) << "c. L1 writes:" << left << setw(10) << L1writes << endl;
    cout << setw(30) << "d. L1 write misses:" << left << setw(10) << L1write_misses << endl;
    cout << setw(30) << "e. L1 miss rate:" << left << setw(10) << fixed << setprecision(4) << L1missRate << endl;
    cout << setw(30) << "f. L1 writebacks:" << left << setw(10) << L1wb << endl;
    cout << setw(30) << "g. L1 prefetches:" << left << setw(10) << L1pref << endl;
    cout << setw(30) << "h. L2 reads (demand):" << left << setw(10) << L2reads << endl;
    cout << setw(30) << "i. L2 read misses (demand):" << left << setw(10) << L2read_misses << endl;
    cout << setw(30) << "j. L2 reads (prefetch):" << left << setw(10) << L2reads_pref << endl;
    cout << setw(30) << "k. L2 read misses (prefetch):" << left << setw(10) << L2read_misses_pref << endl;
    cout << setw(30) << "l. L2 writes:" << left << setw(10) << L2writes << endl;
    cout << setw(30) << "m. L2 write misses:" << left << setw(10) << L2write_misses << endl;
    cout << setw(30) << "n. L2 miss rate:" << left << setw(10) << fixed << setprecision(4) << L2missRate << endl;
    cout << setw(30) << "o. L2 writebacks:" << left << setw(10) << L2wb << endl;
    cout << setw(30) << "p. L2 prefetches:" << left << setw(10) << L2pref << endl;
    cout << setw(30) << "q. memory traffic:" << left << setw(10) << TotalMemtraffic << endl;


    return(0);
}
