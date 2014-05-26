/* 
 * File:   LoadManySchedule.h
 * Author: pnav
 *
 * if a domain has a bunch of rays, add the domain to an empty process. 
 * this starts with the adaptive LoadOnce and keeps assigning a domain to procs until the expected value is under a threshold
 * so at present, it adds the domain to a process that has the most rays pending, which might or might not be a domain already loaded (PAN - 20130710)
 * 
 * Created on February 4, 2014, 3:41 PM
 */

#ifndef GVT_LOADMANYSCHEDULE_H
#define	GVT_LOADMANYSCHEDULE_H

#include <list>

#include "HybridBaseSchedule.h"


struct LoadManySchedule : public HybridBaseSchedule {

    LoadManySchedule(int * newMap, int &size, int *map_size_buf, int **map_recv_bufs, int *data_send_buf) : HybridBaseSchedule(newMap, size, map_size_buf, map_recv_bufs, data_send_buf) {

    }

    virtual ~LoadManySchedule() {

    }

    virtual void operator()() {
        DEBUG(cerr << "in LoadMany schedule" << endl);
        for (int i = 0; i < size; ++i)
            newMap[i] = -1;

        std::map<int, int> data2proc;
        std::map<int, int> data2size;
        std::map<int, int> size2data;
        for (int s = 0; s < size; ++s) {
            if (map_recv_bufs[s]) {
                // add currently loaded data. this will evict previous entries. that's okay since we don't want to dup data, 
                // we want to reset the map to only one proc per domain unless pending rays demand more
                data2proc[map_recv_bufs[s][0]] = data2proc[map_recv_bufs[s][0]] + s;
                DEBUG(cerr << "    noting currently " << s << " -> " << map_recv_bufs[s][0] << endl);

                // add ray counts
                for (int d = 1; d < map_size_buf[s]; d += 2) {
                    data2size[map_recv_bufs[s][d]] += map_recv_bufs[s][d + 1];
                    DEBUG(cerr << "        " << s << " has " << map_recv_bufs[s][d + 1] << " rays for data " << map_recv_bufs[s][d] << endl);
                }
            }
        }

        // convert data2size into size2data, 
        // use data id to pseudo-uniqueify, since only need ordering
        for (std::map<int, int>::iterator it = data2size.begin(); it != data2size.end(); ++it) {
            size2data[(it->second << 7) + it->first] = it->first;
        }

        // iterate over queued data, find which procs have the most rays
        // since size2data is sorted in increasing key order, 
        // data that has the most rays pending will end up at the top of the floater list
        std::vector<int> bloated;
        std::list<int> floaters;
        for (std::map<int, int>::iterator s2dit = size2data.begin(); s2dit != size2data.end(); ++s2dit) {
            map<int, int>::iterator d2pit = data2proc.find(s2dit->second);
            if (d2pit != data2proc.end()) {
                newMap[d2pit->second] = d2pit->first;
                DEBUG(cerr << "    adding " << d2pit->second << " -> " << d2pit->first << " to map" << endl);
            }

            bloated.push_back(s2dit->second);
            DEBUG(cerr << "    noting domain " << s2dit->second << " is bloated with size " << s2dit->first << endl);
        }

        // iterate over newMap, fill as many procs as possible with data
        // keep adding a proc until expected size drops below threshhold
#define MPIRT_FLOATER_THRESHHOLD 100000
        for (int i = 0; i < size; ++i) {
            if (newMap[i] < 0) {
                // if bloat list is empty, pull the top of the float list
                if (bloated.empty()) {
                    if (!floaters.empty()) {
                        newMap[i] = floaters.front();
                        data2proc[newMap[i]] = i;
                        floaters.pop_front();
                    } else break; // we're done here
                } else // !bloated.empty()
                {
                    int entry;
                    if (floaters.empty()) {
                        // use the bloat entry and float it if there's still rays
                        entry = bloated.back();
                        bloated.pop_back();
                    } else // !floaters.empty()
                    {
                        // check if floating elements have more expected rays than the current bloat entry
                        // if so, use a floater instead of the bloat entry
                        if (data2size[bloated.back()] > data2size[floaters.front()]) {
                            entry = bloated.back();
                            bloated.pop_back();
                        }
                        else {
                            entry = floaters.front();
                            floaters.pop_front();
                        }
                    }
                    newMap[i] = entry;
                    data2proc[entry] = i;
                    data2size[entry] = data2size[entry] >> 1; // divide size by 2
                    if (data2size[entry] > MPIRT_FLOATER_THRESHHOLD) {
                        // tack it on the back. this does not guarantee a proper sort within floaters, 
                        // but it's quick and likely good enough
                        floaters.push_back(entry);
                    }
                }
            }
        }

        DEBUG(cerr << "new map size is " << size << endl;
        for (int i = 0; i < size; ++i)
                cerr << "    " << i << " -> " << newMap[i] << endl;
                );
    }
};


#endif	/* GVT_LOADMANYSCHEDULE_H */

