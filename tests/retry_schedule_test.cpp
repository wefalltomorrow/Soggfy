#include "../native/retry_schedule.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    using history::RetrySchedule;
    RetrySchedule<int,2,12> schedule;
    assert(history::transient_retry_limit==3);
    assert(history::transient_retry_delay_ms(0)==2000);
    assert(history::transient_retry_delay_ms(1)==10000);
    assert(history::transient_retry_delay_ms(2)==30000);
    assert(history::transient_retry_delay_ms(3)==0);

    assert(schedule.Push(1,4,100));
    assert(schedule.Push(2,4,50));
    assert(!schedule.Push(3,5,25)); // The byte budget includes both entries.
    assert(!schedule.Push(3,1,25)); // The pending-entry limit is independent.
    assert(schedule.ReservedBytes()==8&&schedule.NextDue()==50);

    int item=0;std::size_t bytes=0;
    assert(!schedule.TakeDue(49,item,bytes));
    assert(schedule.TakeDue(50,item,bytes)&&item==2&&bytes==4);
    assert(schedule.ReservedBytes()==8); // In-flight memory remains charged.
    assert(!schedule.Push(3,1,30)); // The in-flight job also counts toward the entry limit.
    assert(!schedule.TakeDue(100,item,bytes)); // Only one in-flight lease.
    assert(schedule.Requeue(2,4,150));
    assert(schedule.NextDue()==100&&schedule.ReservedBytes()==8);
    assert(schedule.TakeDue(100,item,bytes)&&item==1);
    assert(schedule.ReleaseInFlight(bytes));
    assert(schedule.ReservedBytes()==4);
    assert(schedule.TakeDue(150,item,bytes)&&item==2);
    assert(schedule.ReleaseInFlight(bytes));
    assert(schedule.ReservedBytes()==0&&schedule.Size()==0);

    assert(schedule.Push(4,6,200));
    assert(schedule.Push(5,5,300));
    assert(schedule.Discard(0));
    assert(schedule.Size()==1&&schedule.ReservedBytes()==5&&schedule.NextDue()==300);
    std::cout<<"PASS: bounded retry deadlines, in-flight byte accounting, requeue and cancellation\n";
}
