/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>

#define MAX_ITERATIONS  5000
#define THRESHOLD       0.00010f

#include "benchmark/benchmark.c"

#include "data/data.h"
#include "Golden/gold.h"








typedef union float_hex {
  float f;
  uint32_t ui32;
} float_hex;

#define BOX_FLOAT_IN_FLOAT(VAR_NAME, VAL_32B)                                  \
  do {                                                                         \
    float_hex nan_boxed_val;                                                   \
    nan_boxed_val.ui32 = VAL_32B;                                              \
    VAR_NAME = nan_boxed_val.f;                                                \
  } while (0)

// float *objects[N_OBJECTS];
float *objects;
float *dist;
float *min_dist;
float *newClusters[N_CLUSTERS];
int *newClusterSize;
int *membership;
float *clusters[N_CLUSTERS];
float *temp;
int *temp2;
int *indx;

void check_result(float *x, int r){
    
    float diff = 0.0f;
    int err = 0;

    for(int i = 0; i < r; i++){  
        diff = fabs(x[i] - check[i]);
        if(diff>THRESHOLD){
            err++;
            printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, check[i], x[i], diff);
        }
    }

    if(err != 0)
        printf("TEST FAILED with %d errors!!\n", err);
    else
        printf("TEST PASSED!!\n");
    
    }


int fp_check_32(float val, const float golden) {
    float diff = val - golden;
    if (diff < 0)
        diff = -diff;

    float eps = 0.01f * golden;
    if (eps < 0)
        eps = -eps;

    return ((diff > eps) && (diff > 1e-5));
}

int check_data32(float *a,float *g,int start, int end){
    int errors = 0;
    for (int i = start; i < end; ++i){
        if (fp_check_32((((float *) a)[i]), (((float *) g)[i-start]))) {
            errors ++;
            printf("error detected at index %d. Value = %x. Golden = %x \n", i, ((uint32_t *) a)[i], ((uint32_t *) g)[i-start]);
        }
    }
    return errors;
}

// void find_nearest_cluster(float  *object,      /* [N_COORDS] */
//                          float **clusters,    /* [N_CLUSTERS][N_COORDS] */
//                          unsigned int vl,
//                          int *index)
// {
//     int i;
//     float dist, min_dist;

//     /* find the cluster id that has min distance to object */
//     min_dist=1000000;
//     //ceil(N_COORDS/16) = V (#vectors needed to store the N_COORDS)
//     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128

//     // for (int ii = 0; ii < N_COORDS; ii++){
//     //     printf("object[0][%d] = %f\n",ii,object[ii]);
//     // }

//     asm volatile("vle32.v v0, (%0)" :: "r"(object));
    
//     // asm volatile("vse32.v v0, (%0)" :: "r"(temp));
//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("i = %f\n",temp[i]);
//     // }

//     for(i=0; i<N_CLUSTERS; i++){
//         // printf("find_nearest_cluster inner loop = %d\n",i);

//         asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128
//         // asm volatile("vle32.v v1, (%0)" :: "r"(clusters[i]));

//         switch(i){
//             case 0:
//                 asm volatile("vmv.v.v v1 , v11");break;
//             case 1:
//                 asm volatile("vmv.v.v v1 , v12");break;
//             case 2:
//                 asm volatile("vmv.v.v v1 , v13");break;
//             case 3:
//                 asm volatile("vmv.v.v v1 , v14");break;
//             case 4:
//                 asm volatile("vmv.v.v v1 , v15");break;
//             case 5:
//                 asm volatile("vmv.v.v v1 , v16");break;
//             case 6:
//                 asm volatile("vmv.v.v v1 , v17");break;
//             case 7:
//                 asm volatile("vmv.v.v v1 , v18");break;
//             default:
//                 break;
//         }




//         // for (int j = 0; j < N_COORDS; j++){
//         //     printf("j = %f\n",clusters[i][j]);
//         // }

//         // asm volatile("vse32.v v1, (%0)" :: "r"(temp));
//         // for (int i = 0; i < N_COORDS; i++){
//         //     printf("i = %d => v1 = %f\n",i,temp[i]);
//         // }

//         // asm volatile("vse32.v v0, (%0)" :: "r"(temp));
//         // for (int i = 0; i < N_COORDS; i++){
//         //     printf("i = %d => v0 %f\n",i,temp[i]);
//         // }


//         asm volatile("vfsub.vv v1, v0, v1");
//         asm volatile("vfmul.vv v1, v1, v1");
        
//         // asm volatile("vse32.v v1, (%0)" :: "r"(temp));
//         // for (int i = 0; i < N_COORDS; i++){
//         //     printf("i = %f\n",temp[i]);
//         // }        
        
//         // asm volatile("vfmul.vv v30, v30, v30");//////TODO IT SHOULD BE CHECKED
//         // printf("\n");
//         asm volatile("vfredsum.vs v2, v1, v31");

//         // asm volatile("vse32.v v2, (%0)" :: "r"(temp));
//         // for (int i = 0; i < N_COORDS; i++){
//         //     printf("i = %f\n",temp[i]);
//         // }


//         asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(4));//VLMAX = LMUL*VLEN/SEW =128
//         asm volatile("vse32.v v2, (%0)" :: "r"(temp));
//         // asm volatile("vfmul.vv v31, v31, v31");//////TODO IT SHOULD BE CHECKED

//         // snrt_cluster_hw_barrier();


//         // printf("i = %d\tdist = %f\tmin_dist = %f\n",i,temp[0],min_dist);
//         if (temp[0] < min_dist) { /* find the min and its array index */
//             min_dist = temp[0];
//             index[0]    = i;
//         }
//         // printf("i = %d\tdist = %f\tmin_dist = %f\n",i,temp[0],min_dist);  
//         // asm volatile("vfmul.vv v31, v31, v31");//////TODO IT SHOULD BE CHECKED

      
//     }
//     // return(index);
// }

// void kmeans(){
//     float    delta;          /* % of objects change their clusters */
    
//     int loop;
    
//     unsigned int vl;
    
//     // int i, j, k, index;
//     int i, j, k;
//     int *index;

//     loop = 0;
//     index = &temp2[0];
//     // allocate a 2D space for returning variable clusters[] (coordinates
//     //   of cluster centers)  
//     // for (i=1; i<N_CLUSTERS; i++)
//     //     clusters[i] = clusters[i-1] + N_COORDS;

//     // pick first N_CLUSTERS elements of objects[] as initial cluster centers 
//     // for (i=0; i<N_CLUSTERS; i++)
//     //     for (j=0; j<N_COORDS; j++)
//     //         clusters[i][j] = objects[i][j];


//     // Cs0,Cs1,Cs2,Cs3,Cs4,Cs5,Cs6,Cs7
//     // V11,V12,V13,V14,V15,V16,V17,V18 for clusters (not efficient)
//     float fscalar_16;
//     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128
    
//     asm volatile("vle32.v v11, (%0)" :: "r"(objects[0]));
//     asm volatile("vle32.v v12, (%0)" :: "r"(objects[1]));
//     asm volatile("vle32.v v13, (%0)" :: "r"(objects[2]));
//     asm volatile("vle32.v v14, (%0)" :: "r"(objects[3]));
//     asm volatile("vle32.v v15, (%0)" :: "r"(objects[4]));
//     asm volatile("vle32.v v16, (%0)" :: "r"(objects[5]));
//     asm volatile("vle32.v v17, (%0)" :: "r"(objects[6]));
//     asm volatile("vle32.v v18, (%0)" :: "r"(objects[7]));


//     // asm volatile("vse32.v v11, (%0)" :: "r"(temp));
//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("v11 i = %f\n",temp[i]);
//     // }
//     // asm volatile("vse32.v v12, (%0)" :: "r"(temp));
//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("v12 i = %f\n",temp[i]);
//     // }

//     // initialize membership[]  
//     for (i=0; i<N_OBJECTS; i++) membership[i] = -1;

//     // need to initialize newClusterSize and newClusters[0] to all 0  
//     for (i=0; i<N_CLUSTERS; i++) {
//       newClusterSize[i] = 0;
//     //   for (j=0; j<N_COORDS; j++)
//     //     newClusters[i][j] = 0.0f;
//     }
//     // V0,V1,V2,V31 are used in find_nearest_cluster
//     // Load all newClusters[i] to vector register file
//     // in this case there is 8 clusters => 
//     // C0,C1,C2,C3,C4,C5,C6,C7
//     // V3,V4,V5,V6,V7,V8,V9,V10
//     // The object is already loaded in V0
//     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(2*N_COORDS));//whole v31 is zero
//     asm volatile("vfsub.vv v31, v31, v31");
//     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128
//     asm volatile("vmv.v.v  v3 , v31");
//     asm volatile("vmv.v.v  v4 , v31");
//     asm volatile("vmv.v.v  v5 , v31");
//     asm volatile("vmv.v.v  v6 , v31");
//     asm volatile("vmv.v.v  v7 , v31");
//     asm volatile("vmv.v.v  v8 , v31");
//     asm volatile("vmv.v.v  v9 , v31");
//     asm volatile("vmv.v.v  v10, v31");
//     // asm volatile("vfsub.vv v3 , v31, v31");
//     // asm volatile("vfsub.vv v4 , v31, v31");
//     // asm volatile("vfsub.vv v5 , v31, v31");
//     // asm volatile("vfsub.vv v6 , v31, v31");
//     // asm volatile("vfsub.vv v7 , v31, v31");
//     // asm volatile("vfsub.vv v8 , v31, v31");
//     // asm volatile("vfsub.vv v9 , v31, v31");
//     // asm volatile("vfsub.vv v10, v31, v31");








//     // asm volatile("vse32.v v4, (%0)" :: "r"(temp));
//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("i = %f\n",temp[i]);
//     // }


//     do {
//         // printf("LOOP = %d\n",loop);
//         delta = 0.0f;

//         for (i=0; i<N_OBJECTS; i++) {
//             // find the array index of nestest cluster center 
//             // printf("find_nearest_cluster START\n");
            

//             // index = find_nearest_cluster(objects[i], clusters, vl);           
//             find_nearest_cluster(objects[i], clusters, vl, index);
//             // printf("object = %d\tindex = %d\n",i,*index);
//             // printf("membership = %d\n",membership[i]);
//             // printf("find_nearest_cluster DONE\n");

//             //printf("Index = %d\n", index);

//             // if membership changes, increase delta by 1 

//             // if (membership[i] != index){
//             //     printf("NOT EQ\n");
//             // }
//             // if (membership[i] == index){
//             //     printf("EQ\n");
//             // }


//             if (membership[i] != *index){ delta += 1.0f; /*printf("delta changed\n");*/}
//             // assign the membership to object i 
//             membership[i] = *index;
//             // printf("delta = %f\n",delta);
//             // update new cluster centers : sum of objects located within 
//             newClusterSize[*index]++;


//             // asm volatile("vse32.v v0, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("i = %f\n",temp[i]);
//             // }

//             // V0,V1,V2,V31 are used in find_nearest_cluster
//             // Load all newClusters[i] to vector register file
//             // in this case there is 8 clusters => 
//             // C0,C1,C2,C3,C4,C5,C6,C7
//             // V3,V4,V5,V6,V7,V8,V9,V10
//             // The object is already loaded in V0
//             // newClusters
//             asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128

//             switch(*index){
//                 case 0:
//                     asm volatile("vfadd.vv v3 , v3 , v0");break;
//                 case 1:
//                     asm volatile("vfadd.vv v4 , v4 , v0");break;
//                 case 2:
//                     asm volatile("vfadd.vv v5 , v5 , v0");break;
//                 case 3:
//                     asm volatile("vfadd.vv v6 , v6 , v0");break;
//                 case 4:
//                     asm volatile("vfadd.vv v7 , v7 , v0");break;
//                 case 5:
//                     asm volatile("vfadd.vv v8 , v8 , v0");break;
//                 case 6:
//                     asm volatile("vfadd.vv v9 , v9 , v0");break;
//                 case 7:
//                     asm volatile("vfadd.vv v10, v10, v0");break;
//                 default:
//                     break;
//             }
//         }
//         // average the sum and replace old cluster centers with newClusters

//         // printf("FOR DONE\n");
//         // asm volatile("vse32.v v31, (%0)" :: "r"(temp));
//         // for (int i = 0; i < N_COORDS; i++){
//         //     printf("i = %f\n",temp[i]);
//         // }
        

//         // for (int i = 0; i < N_CLUSTERS; i++){
//         //     printf("newClusterSize[%d] = %d\n",i,newClusterSize[i]);
//         // }


//         if (newClusterSize[0] > 0) {
//             // printf("case0\n");
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, ((uint32_t*)newClusterSize)[0]);
//             // asm volatile("vfdiv.vf v11, v3 , %[A]" ::[A] "f"(fscalar_16));
//             asm volatile("vfdiv.vf v11, v3 , %[A]" ::[A] "f"((float)newClusterSize[0]));
            
//             // printf("float = %f\n",fscalar_16);
//             // printf("newClusterSize[0] = %f\n",(float)newClusterSize[0]);

//             // asm volatile("vse32.v v3, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("v3[%d] = %f\n",i,temp[i]);
//             // }
            
//             // asm volatile("vse32.v v11, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 0 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[1] > 0) {
//             // printf("case1\n");
//             // printf("int = %d\n",newClusterSize[1]);
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, ((uint32_t*)newClusterSize)[1]);
//             // printf("case1 BOX\n");
//             // printf("int = %d\n",newClusterSize[1]);
//             // printf("float = %f\n",(float)newClusterSize[1]);
//             // printf("float = %lx\n",(uint32_t)fscalar_16);
//             // printf("float = %lx\n",((uint32_t*)newClusterSize)[1]);
//             asm volatile("vfdiv.vf v12, v4 , %[A]" ::[A] "f"((float)newClusterSize[1]));
//             // asm volatile("vse32.v v12, (%0)" :: "r"(temp));
//             // printf("cluster 1 center = %f\n",temp);

//             // asm volatile("vse32.v v12, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 1 center = %f\n",temp[i]);
//             // }

//             // printf("case1 Div\n");
//         }
//         if (newClusterSize[2] > 0) {
//             // printf("case2\n");
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[2]);
//             asm volatile("vfdiv.vf v13, v5 , %[A]" ::[A] "f"((float)newClusterSize[2]));
//             // asm volatile("vse32.v v13, (%0)" :: "r"(temp));
//             // printf("cluster 2 center = %f\n",temp);
//             // asm volatile("vse32.v v13, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 2 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[3] > 0) {
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[3]);
//             asm volatile("vfdiv.vf v14, v6 , %[A]" ::[A] "f"((float)newClusterSize[3]));
//             // asm volatile("vse32.v v14, (%0)" :: "r"(temp));
//             // printf("cluster 3 center = %f\n",temp);
//             // printf("newClusterSize[3] = %d\n",newClusterSize[3]);

//             // asm volatile("vse32.v v6, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("v6 = %f\n",temp[i]);
//             // }
//             // asm volatile("vse32.v v14, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 3 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[4] > 0) {
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[4]);
//             asm volatile("vfdiv.vf v15, v7 , %[A]" ::[A] "f"((float)newClusterSize[4]));
//             // asm volatile("vse32.v v15, (%0)" :: "r"(temp));
//             // printf("cluster 4 center = %f\n",temp);
//             // asm volatile("vse32.v v15, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 4 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[5] > 0) {
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[5]);
//             asm volatile("vfdiv.vf v16, v8 , %[A]" ::[A] "f"((float)newClusterSize[5]));
//             // asm volatile("vse32.v v16, (%0)" :: "r"(temp));
//             // printf("cluster 5 center = %f\n",temp);
//             // asm volatile("vse32.v v16, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 5 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[6] > 0) {
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[6]);
//             asm volatile("vfdiv.vf v17, v9 , %[A]" ::[A] "f"((float)newClusterSize[6]));
//             // asm volatile("vse32.v v17, (%0)" :: "r"(temp));
//             // printf("cluster 6 center = %f\n",temp);
//             // asm volatile("vse32.v v17, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 6 center = %f\n",temp[i]);
//             // }
//         }
//         if (newClusterSize[7] > 0) {
//             // BOX_FLOAT_IN_FLOAT(fscalar_16, (float)newClusterSize[7]);
//             asm volatile("vfdiv.vf v18, v10, %[A]" ::[A] "f"((float)newClusterSize[7]));
//             // asm volatile("vse32.v v18, (%0)" :: "r"(temp));
//             // for (int i = 0; i < N_COORDS; i++){
//             //     printf("cluster 7 center = %f\n",temp[i]);
//             // }
//         }
        
//         // printf("vmv Start\n");  
//         asm volatile("vmv.v.v  v3 , v31");
//         asm volatile("vmv.v.v  v4 , v31");
//         asm volatile("vmv.v.v  v5 , v31");
//         asm volatile("vmv.v.v  v6 , v31");
//         asm volatile("vmv.v.v  v7 , v31");
//         asm volatile("vmv.v.v  v8 , v31");
//         asm volatile("vmv.v.v  v9 , v31");
//         asm volatile("vmv.v.v  v10, v31");        

//         // asm volatile("vfsub.vv v3 , v31, v31");
//         // asm volatile("vfsub.vv v4 , v31, v31");
//         // asm volatile("vfsub.vv v5 , v31, v31");
//         // asm volatile("vfsub.vv v6 , v31, v31");
//         // asm volatile("vfsub.vv v7 , v31, v31");
//         // asm volatile("vfsub.vv v8 , v31, v31");
//         // asm volatile("vfsub.vv v9 , v31, v31");
//         // asm volatile("vfsub.vv v10, v31, v31");


//         // printf("vmv Done\n");
        
//         // for (i=0; i<N_CLUSTERS; i++) {
//         //     for (j=0; j<N_COORDS; j++) {
//         //         if (newClusterSize[i] > 0) {
//         //             clusters[i][j] = newClusters[i][j] / newClusterSize[i];
//         //             }
                    

//         //         newClusters[i][j] = 0.0f;   // set back to 0 
//         //     }
//         //     newClusterSize[i] = 0;   // set back to 0 
//         // }
//         for (i=0; i<N_CLUSTERS; i++) {
//             newClusterSize[i] = 0;   // set back to 0 
//         }

//         delta /= N_OBJECTS;
//         // printf("delta = %f\n",delta);
//         loop++;

//     } while (delta > (float)THRESHOLD && loop < MAX_ITERATIONS);

//     asm volatile("vsetvli %0 , %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));


//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("clusters[0][%d] = %f\n",i,clusters[0][i]);
//     // }

//     asm volatile("vse32.v v11 , (%0)" :: "r"(clusters[0]));

//     // for (int i = 0; i < N_COORDS; i++){
//     //     printf("clusters[0][%d] = %f\n",i,clusters[0][i]);
//     // }

//     asm volatile("vse32.v v12, (%0)" :: "r"(clusters[1]));
//     asm volatile("vse32.v v13, (%0)" :: "r"(clusters[2]));
//     asm volatile("vse32.v v14, (%0)" :: "r"(clusters[3]));
//     asm volatile("vse32.v v15, (%0)" :: "r"(clusters[4]));
//     asm volatile("vse32.v v16, (%0)" :: "r"(clusters[5]));
//     asm volatile("vse32.v v17, (%0)" :: "r"(clusters[6]));
//     asm volatile("vse32.v v18, (%0)" :: "r"(clusters[7]));
//     return;
// }





void kmeans_v2(){
    float    delta = N_OBJECTS;          /* % of objects change their clusters */
    
    int loop;
    
    unsigned int vl;
    
    int i, j, k;

    loop = 0;
    // index = &temp2[0];

    // // Cs0,Cs1,Cs2,Cs3,Cs4,Cs5,Cs6,Cs7
    // // V11,V12,V13,V14,V15,V16,V17,V18 for clusters (not efficient)
    // float fscalar_16;
    // asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128
    
    // asm volatile("vle32.v v11, (%0)" :: "r"(objects[0]));
    // asm volatile("vle32.v v12, (%0)" :: "r"(objects[1]));
    // asm volatile("vle32.v v13, (%0)" :: "r"(objects[2]));
    // asm volatile("vle32.v v14, (%0)" :: "r"(objects[3]));
    // asm volatile("vle32.v v15, (%0)" :: "r"(objects[4]));
    // asm volatile("vle32.v v16, (%0)" :: "r"(objects[5]));
    // asm volatile("vle32.v v17, (%0)" :: "r"(objects[6]));
    // asm volatile("vle32.v v18, (%0)" :: "r"(objects[7]));

    // // initialize membership[]  
    // for (i=0; i<N_OBJECTS; i++) membership[i] = -1;

    // // need to initialize newClusterSize and newClusters[0] to all 0  
    // for (i=0; i<N_CLUSTERS; i++) {
    //   newClusterSize[i] = 0;
    // }
    // // V0,V1,V2,V31 are used in find_nearest_cluster
    // // Load all newClusters[i] to vector register file
    // // in this case there is 8 clusters => 
    // // C0,C1,C2,C3,C4,C5,C6,C7
    // // V3,V4,V5,V6,V7,V8,V9,V10
    // // The object is already loaded in V0
    // asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(2*N_COORDS));//whole v31 is zero
    // asm volatile("vfsub.vv v31, v31, v31");
    // asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N_COORDS));//VLMAX = LMUL*VLEN/SEW =128
    // asm volatile("vmv.v.v  v3 , v31");
    // asm volatile("vmv.v.v  v4 , v31");
    // asm volatile("vmv.v.v  v5 , v31");
    // asm volatile("vmv.v.v  v6 , v31");
    // asm volatile("vmv.v.v  v7 , v31");
    // asm volatile("vmv.v.v  v8 , v31");
    // asm volatile("vmv.v.v  v9 , v31");
    // asm volatile("vmv.v.v  v10, v31");

    // pick first N_CLUSTERS elements of objects[] as initial cluster centers 
    // for (i=0; i<N_CLUSTERS; i++)
    //     for (j=0; j<N_COORDS; j++){
    //         clusters[i][j] = objects[i*N_COORDS+j];
    //     }

    int itr = 0;
    int num_elements = N_COORDS;
    int VLMAX = 128;
    
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
      for (int i = 0; i < N_CLUSTERS; i++){
        asm volatile("vle32.v v0, (%0)" :: "r"(objects+i*N_COORDS+itr*VLMAX));
        asm volatile("vse32.v v0, (%0)" :: "r"(clusters[i]+itr*VLMAX));
      }
      itr++;
    }
    if(itr*VLMAX < num_elements){
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      for (int i = 0; i < N_CLUSTERS; i++){
        asm volatile("vle32.v v0, (%0)" :: "r"(objects+i*N_COORDS+itr*VLMAX));
        asm volatile("vse32.v v0, (%0)" :: "r"(clusters[i]+itr*VLMAX));
      }
    }
    // printf("done1\n");


    // initialize membership[]  
    // for (i=0; i<N_OBJECTS; i++) membership[i] = -1;

    itr = 0;
    num_elements = N_OBJECTS;
    VLMAX = 128;
    const int32_t scalar = 1;

    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vsub.vv  v0, v0, v0");
    asm volatile("vsub.vx  v0, v0, %[A]" ::[A] "r"(scalar));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vse32.v v0, (%0)" :: "r"(membership+itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vse32.v v0, (%0)" :: "r"(membership+itr*VLMAX));
    }
    // printf("done2\n");



    // need to initialize newClusterSize and newClusters[0] to all 0  
    // for (i=0; i<N_CLUSTERS; i++) {
    //   newClusterSize[i] = 0;
    //   for (j=0; j<N_COORDS; j++)
    //     newClusters[i][j] = 0.0f;
    // }

    itr = 0;
    num_elements = N_CLUSTERS;
    VLMAX = 128;

    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vsub.vv  v0, v0, v0");
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vse32.v v0, (%0)" :: "r"(newClusterSize+itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vse32.v v0, (%0)" :: "r"(newClusterSize+itr*VLMAX));
    }
    // printf("done3\n");


    itr = 0;
    num_elements = N_COORDS;
    VLMAX = 128;

    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vsub.vv  v0, v0, v0");
    for (i=0; i<N_CLUSTERS; i++) {
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vse32.v v0, (%0)" :: "r"(newClusters[i]+itr*VLMAX));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vse32.v v0, (%0)" :: "r"(newClusters[i]+itr*VLMAX));
        }
    }
    // printf("done4\n");



    while (delta > 0 && loop < MAX_ITERATIONS){
    // while (delta > 0 && loop < 13){
        delta = 0;
        // printf("LOOP=%d\n",loop);
        int itr = 0;
        int num_elements = N_OBJECTS;
        int VLMAX = 128;
        int stridesize = N_COORDS*4;//N_COORDS * 4 byte

        const uint32_t scalar = 0x7F800000;//+inf

        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        // asm volatile("vfsub.vv  v8, v8, v8");
        while(itr*VLMAX + VLMAX <= num_elements){
            // asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(scalar));
            for (int i = 0; i < N_CLUSTERS; i++){
                // printf("N_CLUSTERS=%d\n",i);
                asm volatile("vfsub.vv  v8, v8, v8");
                for (int j = 0; j < N_COORDS; j++){
                    // printf("N_COORDS=%d\n",j);
                    float clusterCoord = clusters[i][j]; 
                    asm volatile("vlse32.v  v0 , (%0), %1" :: "r"(objects+itr*VLMAX+j), "r"(stridesize));//EVEN indices
                    asm volatile("vfsub.vf  v0 , v0, %0" ::"f"(clusterCoord));
                    asm volatile("vfmacc.vv v8 , v0, v0");
                }
                asm volatile("vse32.v   v8, (%0)" :: "r"(dist+itr*VLMAX));
                for (int k = 0; k < VLMAX; k++){
                    if(dist[k+itr*VLMAX] < min_dist[k+itr*VLMAX]){
                        min_dist[k+itr*VLMAX] = dist[k+itr*VLMAX];
                        indx[k+itr*VLMAX]=i;
                    }
                }                
            }
        itr++;
        }
        if(itr*VLMAX < num_elements){
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            for (int i = 0; i < N_CLUSTERS; i++){
                asm volatile("vfsub.vv  v8, v8, v8");
                for (int j = 0; j < N_COORDS; j++){
                    float clusterCoord = clusters[i][j]; 
                    asm volatile("vlse32.v  v0 , (%0), %1" :: "r"(objects+itr*VLMAX+j), "r"(stridesize));//EVEN indices        
                    asm volatile("vfsub.vf  v0 , v0, %0" ::"f"(clusterCoord));
                    asm volatile("vfmacc.vv v8 , v0, v0");
                }
                asm volatile("vse32.v   v8, (%0)" :: "r"(dist+itr*VLMAX));
                for (int k = 0; k < num_elements - itr*VLMAX; k++){
                    if(dist[k+itr*VLMAX] < min_dist[k+itr*VLMAX]){
                        min_dist[k+itr*VLMAX] = dist[k+itr*VLMAX];
                        indx[k+itr*VLMAX]=i;
                    }
                }
                // printf("dist[3][%d] = %f\n",i,dist[3]);
            }
        }


        

        // if(loop ==1){
        //     for (int i = 0; i < N_OBJECTS; i++){
        //         printf("indx[%d] = %d\n",i,indx[i]);
        //     }
        // }
        
        // printf("WHILE DONE\n");
        

        // for (int i = 0; i < N_OBJECTS; i++){
        //     if (membership[i] != indx[i]){ 
        //         delta += 1.0f; 
        //     }
        //     membership[i] = indx[i];
        //     newClusterSize[indx[i]]++;
        //     for (j=0; j<N_COORDS; j++) {
        //         newClusters[indx[i]][j] += objects[i*N_COORDS+j];
        //     }
        // }






        num_elements = N_COORDS;
        VLMAX = 128;
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));

        for (int i = 0; i < N_OBJECTS; i++){
        
            if (membership[i] != indx[i]){ 
                delta += 1.0f; 
            }
            // membership[i] = indx[i];
            newClusterSize[indx[i]]++;
            for (j=0; j<N_COORDS; j++) {
                newClusters[indx[i]][j] += objects[i*N_COORDS+j];
            }
            // itr = 0;
            // while(itr*VLMAX + VLMAX <= num_elements){
            //     asm volatile("vle32.v v0, (%0)" :: "r"(objects+i*N_COORDS+itr*VLMAX));
            //     asm volatile("vle32.v v8, (%0)" :: "r"(newClusters[indx[i]]+itr*VLMAX));
            //     asm volatile("vfadd.vv v8 , v0, v8");
            //     asm volatile("vse32.v v8, (%0)" :: "r"(newClusters[indx[i]]+itr*VLMAX));
            //     itr++;
            // }
            // if(itr*VLMAX < num_elements){
            //     asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            //     asm volatile("vle32.v v0, (%0)" :: "r"(objects+i*N_COORDS+itr*VLMAX));
            //     asm volatile("vle32.v v8, (%0)" :: "r"(newClusters[indx[i]]+itr*VLMAX));
            //     asm volatile("vfadd.vv v8 , v0, v8");
            //     asm volatile("vse32.v v8, (%0)" :: "r"(newClusters[indx[i]]+itr*VLMAX));
            // }
        }

        itr = 0;
        num_elements = N_OBJECTS;
        VLMAX = 128;
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vle32.v v0, (%0)" :: "r"(indx+itr*VLMAX));
            asm volatile("vse32.v v0, (%0)" :: "r"(membership+itr*VLMAX));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vle32.v v0, (%0)" :: "r"(indx+itr*VLMAX));
            asm volatile("vse32.v v0, (%0)" :: "r"(membership+itr*VLMAX));
        }



        // printf("delta = %f\n",delta);
        // printf("FIRST FOR DONE\n");
        // for (i=0; i<N_CLUSTERS; i++) {
        //     for (j=0; j<N_COORDS; j++) {
        //         if (newClusterSize[i] > 0) {
        //             clusters[i][j] = newClusters[i][j] / newClusterSize[i];
        //         }
        //         newClusters[i][j] = 0.0f;   // set back to 0 
        //     }
        //     newClusterSize[i] = 0;   // set back to 0 
        // }
        
        VLMAX = 64;
        itr = 0;
        num_elements = N_COORDS;
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        asm volatile("vfsub.vv v0, v0, v0");

        for (i=0; i<N_CLUSTERS; i++) {
            float NCSize = newClusterSize[i];
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v8, (%0)" :: "r"(newClusters[i]+itr*VLMAX));
                asm volatile("vfdiv.vf v8, v8 , %[A]" ::[A] "f"(NCSize));
                asm volatile("vse32.v v8, (%0)" :: "r"(clusters[i]+itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v8, (%0)" :: "r"(newClusters[i]+itr*VLMAX));
                asm volatile("vfdiv.vf v8, v8 , %[A]" ::[A] "f"(NCSize));
                asm volatile("vse32.v v8, (%0)" :: "r"(clusters[i]+itr*VLMAX));
            }
            asm volatile("vse32.v v0, (%0)" :: "r"(newClusters[i]+itr*VLMAX));
            newClusterSize[i] = 0;   // set back to 0 
        }

        // for (int i = 0; i < N_OBJECTS; i++){
        //     min_dist[i] = 1000;
        // }
        itr = 0;
        num_elements = N_OBJECTS;
        VLMAX = 128;

        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        asm volatile("vfsub.vv  v0, v0, v0");
        asm volatile("vfadd.vf  v0, v0, %0" ::"f"(1000.0f));
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vse32.v v0, (%0)" :: "r"(min_dist+itr*VLMAX));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vse32.v v0, (%0)" :: "r"(min_dist+itr*VLMAX));
        }




        // for (int i = 0; i < N_CLUSTERS; i++){
        //     for (int j = 0; j < N_COORDS; j++){
        //         printf("clusters[%d][%d] = %f\n",i,j,clusters[i][j]);        
        //     }
        // }
        


        // printf("SECOND FOR DONE\n");
        delta /= N_OBJECTS;
        loop++;
    }
    return;
}


int main() {

    volatile int errors = 0;

    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;

    printf("start_addr: %x \n", snrt_cluster_memory().end);

    int reorder_enable = 1;

    if (cid == 0) {
        temp2 = (int *)snrt_l1alloc(1 * sizeof(int));
        for (int i = 0; i < N_CLUSTERS; i++){
            newClusters[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
        }

        newClusterSize = (int   *)snrt_l1alloc(N_CLUSTERS * sizeof(int));
        membership     = (int   *)snrt_l1alloc(N_OBJECTS * sizeof(int));

        dist           = (float *)snrt_l1alloc(N_OBJECTS * sizeof(float));
        min_dist       = (float *)snrt_l1alloc(N_OBJECTS * sizeof(float));
        for (int i = 0; i < N_OBJECTS; i++){
            min_dist[i] = 1000;
        }
        

        for (int i = 0; i < N_CLUSTERS; i++){
            clusters[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
            snrt_dma_start_1d(clusters[i], clusters_dram, N_COORDS* sizeof(float));
        }
        temp     = (float   *)snrt_l1alloc(N_COORDS * sizeof(float));

        // for (int i = 0; i < N_OBJECTS; i++){
        //     objects[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
        //     snrt_dma_start_1d(objects[i], objects_dram[i], N_COORDS* sizeof(float));
        // }

        objects = (float *)snrt_l1alloc(N_COORDS*N_OBJECTS * sizeof(float));
        snrt_dma_start_1d(objects, objects_dram, N_COORDS*N_OBJECTS* sizeof(float));

        indx     = (int   *)snrt_l1alloc(N_OBJECTS * sizeof(int));
        for (int i = 0; i < N_OBJECTS; i++){
            indx[i] = 0;
        }
        


    }

    snrt_cluster_hw_barrier();

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    // printf("kmeans Start\n");
    // kmeans();
    kmeans_v2();
    // printf("kmeans Done\n");
    // End dump
    stop_kernel();

    //     if(cid == 0){
    //         printf("CHECK RESULTS\n");            
    //         errors = check_data32((float *)result,(float *)z_temp2_GOLD,0,2*N_DATA);
    //     }


    // // End timer and check if new best runtime
    // if (cid == 0)
    //     timer = benchmark_get_cycle() - timer;

    // // Display runtime
    // if (cid == 0) {
    //     long unsigned int performance =
    //         1000 * 10 * N_DATA * 6 / 5 / timer;
    //     long unsigned int utilization = performance / (2 * num_cores * 8);

    //     printf("\n----- WUS_CORE on %d samples and %d channels -----\n", N_DATA, DATA_CH);
    //     printf("The execution took %u cycles.\n", timer);
    //     printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
    //         performance, utilization);
    // }



    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    snrt_cluster_hw_barrier();
    // Display runtime
    if (cid == 0) {
        printf("The execution took %u cycles.\n", timer);
    }

    float check_vect[N_CLUSTERS*N_COORDS];
    int c = 0;
    if(cid == 0)
    {    
        
        // for (int i = 0; i < N_COORDS; i++){
        //     printf("clusters[0][%d] = %f\n",i,clusters[0][i]);
        // }

        for (int i=0; i<N_CLUSTERS; i++) {
            for (int j=0; j<N_COORDS; j++) {
                check_vect[c] = clusters[i][j];
                // printf("clusters[%d][%d] = %f\n",i,j,clusters[i][j]);   
                c++;
            }   
        }

        // for (int i = 0; i < N_CLUSTERS*N_COORDS; i++){
        //     // printf("check_vect[%d] = %f\n",i,check_vect[i]);   
        //     printf("check_vect[%d] = %f\n",i,check_vect[i]);   
        // }
        check_result(check_vect, N_CLUSTERS*N_COORDS);
    }




    snrt_cluster_hw_barrier();
    return errors;

}
