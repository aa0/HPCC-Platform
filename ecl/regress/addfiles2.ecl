/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */


namesRecord :=
            RECORD
string20        surname;
string10        forename;
integer2        age := 25;
            END;

namesTable := group(dataset('x',namesRecord,FLAT), surname);
namesTable2 := group(dataset('y',namesRecord,FLAT),forename);
namesTable3 := dataset('z',namesRecord,FLAT);
namesTable4 := group(dataset('z',namesRecord,FLAT), forename);

output(namesTable + namesTable2 + namesTable3);

output(dedup(namesTable + namesTable2 + namesTable4));
