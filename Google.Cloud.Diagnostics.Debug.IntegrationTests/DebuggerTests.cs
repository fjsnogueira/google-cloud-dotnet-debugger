﻿// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System.Net.Http;
using System.Threading.Tasks;
using Xunit;

namespace Google.Cloud.Diagnostics.Debug.IntegrationTests
{
    public class DebuggerTests : DebuggerTestBase
    {
        public DebuggerTests() : base() { }

        [Fact]
        public async Task BreakpointHit()
        {
            using (StartTestApp(debugEnabled: true))
            {
                var debuggee = Polling.GetDebuggee(Module, Version);
                var breakpoint = SetBreakpoint(debuggee.Id, "MainController.cs", 25);

                using (HttpClient client = new HttpClient())
                {
                    await client.GetAsync(AppUrlBase);
                }

                var newBp = Polling.GetBreakpoint(debuggee.Id, breakpoint.Id);

                // Check that the breakpoint has been hit.
                Assert.True(newBp.IsFinalState);
            }           
        }
    }
}