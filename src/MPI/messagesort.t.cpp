
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
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

#include "messagesort.hpp"
#include "config.hpp"

#include <gtest/gtest.h>

using namespace lpf;

typedef MessageSort::MsgId Id;
const size_t G = (1 << Config::instance().getMpiMsgSortGrainSizePower() );

TEST( MessageSort, empty )
{
    MessageSort empty;

}

/** 
 * \pre P >= 1
 * \pre P <= 1
 */
TEST( MessageSort, oneMsg )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[0], array.size()-G );

    {  size_t o = 1*G, s = 4 * G;
        sort.pushWrite( 0, 0, o, s);
        EXPECT_EQ( 1*G, o );
        EXPECT_EQ( 4*G, s );
    }


    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 1*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, oneMsgUnaligned )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[1], array.size()-G );

    {  size_t o = 2*G+10, s = 4 * G+20;
        sort.pushWrite( 0, 0, o, s);
        EXPECT_EQ( 3*G, o );
        EXPECT_EQ( 3*G, s );
    }


    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[1] + 3*G , writeBase );
    EXPECT_EQ( 3*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


TEST( MessageSort, twoDisjointMsgs )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[0], array.size()-G );

    { size_t o = 1*G, s=4*G;
    sort.pushWrite( 0, 0, o , s);
    EXPECT_EQ( 1*G, o ); EXPECT_EQ( 4*G, s);
    o = 8*G; s = 10*G;
    sort.pushWrite( 1, 0, o, s);
    EXPECT_EQ( 8*G, o ); EXPECT_EQ( 10*G, s); }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(0u, msgId );
    EXPECT_EQ( &array[0] + 1*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(1u, msgId );
    EXPECT_EQ( &array[0] + 8*G , writeBase );
    EXPECT_EQ( 10*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, twoAdjacentMsgs )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[0], array.size()-G );

    { size_t o = 2*G, s=11*G;
    sort.pushWrite( 0, 0, o, s);
    EXPECT_EQ( 2*G, o );
    EXPECT_EQ( 11*G, s );
    o=13*G; s=5*G;
    sort.pushWrite( 1, 0, o, s);
    EXPECT_EQ( 13*G, o);
    EXPECT_EQ( 5*G, s); }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(0u, msgId );
    EXPECT_EQ( &array[0] + 2*G , writeBase );
    EXPECT_EQ( 11*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(1u, msgId );
    EXPECT_EQ( &array[0] + 13*G , writeBase );
    EXPECT_EQ( 5*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, twoOverlappingMsgs )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[0], array.size()-G );

    { size_t o=1*G, s=11*G;
      sort.pushWrite( 0, 0, o, s);
      EXPECT_EQ( 1*G, o ); 
      EXPECT_EQ( 11*G, s );
      o=8*G; s=10*G;
      sort.pushWrite( 1, 0, o, s);
      EXPECT_EQ( 8*G, o );
      EXPECT_EQ( 10*G, s );
    }


    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(0u, msgId );
    EXPECT_EQ( &array[0] + 1*G , writeBase );
    EXPECT_EQ( 7*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(1u, msgId );
    EXPECT_EQ( &array[0] + 8*G , writeBase );
    EXPECT_EQ( 10*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, twoOverlappingMsgsPriority )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 1 );
    sort.addRegister( 0, &array[0], array.size()-G );

    { size_t o=8*G, s=10*G;
      sort.pushWrite( 1, 0, o, s);
      EXPECT_EQ( 8*G, o ); 
      EXPECT_EQ( 10*G, s );
      o=1*G; s=11*G;
      sort.pushWrite( 0, 0, o, s);
      EXPECT_EQ( 1*G, o );
      EXPECT_EQ( 11*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(0u, msgId );
    EXPECT_EQ( &array[0] + 1*G , writeBase );
    EXPECT_EQ( 7*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ(1u, msgId );
    EXPECT_EQ( &array[0] + 8*G , writeBase );
    EXPECT_EQ( 10*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


TEST( MessageSort, TwoDisjointRegsAndTwoMsgs )
{
    std::vector< char > array( 50 * G);
    MessageSort sort;
    
    sort.setSlotRange( 2 );
    sort.addRegister( 0, &array[0], 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );

    { size_t o = 0*G, s = 4*G;
      sort.pushWrite( 0, 0, o , s);
      EXPECT_EQ( 0*G, o );
      EXPECT_EQ( 4*G, s );
    }
    { size_t o = 3*G, s = 4*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ( 3*G, o );
      EXPECT_EQ( 4*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 23*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


TEST( MessageSort, ThreeDisjointRegsAndThreeMsgs )
{
    std::vector< char > array( 100 * G);
    MessageSort sort;
    
    sort.setSlotRange( 3 );
    sort.addRegister( 0, &array[0], 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );
    sort.addRegister( 2, &array[0]+70*G, 25*G );
  
    { size_t o = 2*G, s=4*G;
    sort.pushWrite( 0, 0, o , s);
    EXPECT_EQ( 2*G, o );
    EXPECT_EQ( 4*G, s ); }

    { size_t o = 10*G, s = 4*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ( 10*G, o) ;
      EXPECT_EQ( 4*G, s );
    }
    { size_t o = 24*G, s=1*G;
      sort.pushWrite( 2, 2, o, s);
      EXPECT_EQ( 24*G, o );
      EXPECT_EQ( 1*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 2*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 2u, msgId );
    EXPECT_EQ( &array[0] + 94*G , writeBase );
    EXPECT_EQ( 1*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 30*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, ThreeDisjointAndOneHugeOverlapRegsAndThreeMsgs )
{
    std::vector< char > array( 100 * G);
    MessageSort sort;
    
    sort.setSlotRange( 4 );
    sort.addRegister( 0, &array[0]+1*G, 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );
    sort.addRegister( 2, &array[0]+70*G, 25*G );
    sort.addRegister( 3, &array[0], 99*G );

    { size_t o = 2*G, s=4*G;
    sort.pushWrite( 0, 0, o , s);
    EXPECT_EQ( 2*G, o );
    EXPECT_EQ( 4*G, s ); }

    { size_t o = 10*G, s = 4*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ( 10*G, o) ;
      EXPECT_EQ( 4*G, s );
    }
    { size_t o = 24*G, s=1*G;
      sort.pushWrite( 2, 2, o, s);
      EXPECT_EQ( 24*G, o );
      EXPECT_EQ( 1*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 3*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 30*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 2u, msgId );
    EXPECT_EQ( &array[0] + 94*G , writeBase );
    EXPECT_EQ( 1*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


TEST( MessageSort, TheeDisjointAndOneOverlapRegsAndThreeMsgs )
{
    std::vector< char > array( 100 * G);
    MessageSort sort;
    
    sort.setSlotRange( 4 );
    sort.addRegister( 0, &array[0]+1*G, 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );
    sort.addRegister( 2, &array[0]+70*G, 25*G );
    sort.addRegister( 3, &array[0]+30*G, 99*G );

    { size_t o = 2*G, s=4*G;
      sort.pushWrite( 0, 0, o, s);
      EXPECT_EQ( 2*G, o);
      EXPECT_EQ( 4*G, s); }

    { size_t o = 5*G, s=10*G;
      sort.pushWrite( 1, 1, o , s);
      EXPECT_EQ( 5*G, o );
      EXPECT_EQ( 10*G, s );
    }
    { size_t o = 1*G, s=1*G;
      sort.pushWrite( 2, 3, o, s);
      EXPECT_EQ( 1*G, o );
      EXPECT_EQ( 1*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 3*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 25*G , writeBase );
    EXPECT_EQ( 6*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 2u, msgId );
    EXPECT_EQ( &array[0] + 31*G , writeBase );
    EXPECT_EQ( 1*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 32*G , writeBase );
    EXPECT_EQ( 3*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


TEST( MessageSort, clear )
{
    std::vector< char > array( 100 * G);
    MessageSort sort;
    
    sort.setSlotRange( 4 );
    sort.addRegister( 0, &array[0]+1*G, 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );
    sort.addRegister( 2, &array[0]+70*G, 25*G );
    sort.addRegister( 3, &array[0]+30*G, 99*G );

    { size_t o = 2*G, s=4*G;
    sort.pushWrite( 0, 0, o , s);
    EXPECT_EQ( 2*G, o );
    EXPECT_EQ( 4*G, s ); }

    { size_t o = 10*G, s = 4*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ( 10*G, o) ;
      EXPECT_EQ( 4*G, s );
    }
    { size_t o = 24*G, s=1*G;
      sort.pushWrite( 2, 2, o, s);
      EXPECT_EQ( 24*G, o );
      EXPECT_EQ( 1*G, s );
    }

    sort.clear();

    { size_t o = 2*G, s=4*G;
      sort.pushWrite( 0, 0, o, s);
      EXPECT_EQ( 2*G, o);
      EXPECT_EQ( 4*G, s); }

    { size_t o = 5*G, s=10*G;
      sort.pushWrite( 1, 1, o , s);
      EXPECT_EQ( 5*G, o );
      EXPECT_EQ( 10*G, s );
    }
    { size_t o = 1*G, s=1*G;
      sort.pushWrite( 2, 3, o, s);
      EXPECT_EQ( 1*G, o );
      EXPECT_EQ( 1*G, s );
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 3*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 25*G , writeBase );
    EXPECT_EQ( 6*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 2u, msgId );
    EXPECT_EQ( &array[0] + 31*G , writeBase );
    EXPECT_EQ( 1*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 32*G , writeBase );
    EXPECT_EQ( 3*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}

TEST( MessageSort, deleteReg )
{
    std::vector< char > array( 100 * G);
    MessageSort sort;
    
    sort.setSlotRange( 4 );
    sort.addRegister( 0, &array[0]+1*G, 10*G );
    sort.addRegister( 1, &array[0]+20*G, 20*G );
    sort.addRegister( 2, &array[0]+70*G, 25*G );
    sort.addRegister( 3, &array[0]+30*G, 99*G );

    { size_t o = 2*G, s=4*G;
    sort.pushWrite( 0, 0, o , s);
    EXPECT_EQ( 2*G, o );
    EXPECT_EQ( 4*G, s ); }

    { size_t o = 10*G, s = 4*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ( 10*G, o) ;
      EXPECT_EQ( 4*G, s );
    }
    { size_t o = 24*G, s=1*G;
      sort.pushWrite( 2, 2, o, s);
      EXPECT_EQ( 24*G, o );
      EXPECT_EQ( 1*G, s );
    }


    sort.clear();
    sort.delRegister( 2 );
    sort.addRegister( 2, &array[0]+0*G, 25*G );

    { size_t o = 2*G, s=4*G;
    sort.pushWrite( 0, 2, o, s);
    EXPECT_EQ( 2*G, o);
    EXPECT_EQ( 4*G, s);
    }
    { size_t o = 5*G, s=10*G;
      sort.pushWrite( 1, 1, o, s);
      EXPECT_EQ(5*G, o);
      EXPECT_EQ(10*G, s);
    }
    { size_t o = 1*G, s=1*G;
      sort.pushWrite( 2, 3, o, s);
      EXPECT_EQ( 1*G, o);
      EXPECT_EQ( 1*G, s);
    }

    char * writeBase = 0; size_t writeSize; Id msgId=0;
    bool s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 0u, msgId );
    EXPECT_EQ( &array[0] + 2*G , writeBase );
    EXPECT_EQ( 4*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 25*G , writeBase );
    EXPECT_EQ( 6*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 2u, msgId );
    EXPECT_EQ( &array[0] + 31*G , writeBase );
    EXPECT_EQ( 1*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_TRUE( s );
    EXPECT_EQ( 1u, msgId );
    EXPECT_EQ( &array[0] + 32*G , writeBase );
    EXPECT_EQ( 3*G, writeSize );

    s = sort.popWrite( msgId, writeBase, writeSize );
    EXPECT_FALSE( s );
}


