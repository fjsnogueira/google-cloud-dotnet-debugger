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

using Google.Protobuf;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Google.Cloud.Diagnostics.Debug
{
    /// <summary>
    /// A server to read and write breakpoints with clients.
    /// </summary>
    public class BreakpointServer : IBreakpointServer
    {
        /// <summary>A semaphore to protect the buffer.</summary>
        private readonly SemaphoreSlim _semaphore = new SemaphoreSlim(1);

        /// <summary>A buffer to store partial breakpoint messages.</summary>
        private List<byte> _buffer = new List<byte>();

        /// <summary>The pipe to send and receive breakpoint messages with.</summary>
        private readonly INamedPipeServer _pipe;

        /// <summary>
        /// Create a <see cref="BreakpointServer"/>.
        /// </summary>
        /// <param name="pipe">The named pipe to send and receive breakpoint messages with.</param>
        public BreakpointServer(INamedPipeServer pipe) =>_pipe = pipe;

        /// <inheritdoc />
        public Task WaitForConnectionAsync() => _pipe.WaitForConnectionAsync();

        /// <inheritdoc />
        public async Task<Breakpoint> ReadBreakpointAsync(CancellationToken cancellationToken = default(CancellationToken))
        {
            await _semaphore.WaitAsync(cancellationToken).ConfigureAwait(false);
            try
            {
                List<byte> previousBuffer = _buffer;
                _buffer = new List<byte>();

                int bytesWrittenSoFar = 0;
                // Check if we have a full breakpoint message in the buffer.
                // If so just use it and do not try and read another breakpoint.
                int endIndex = IndexOfSequence(previousBuffer.ToArray(), Constants.EndBreakpointMessage);
                while (endIndex == -1)
                {
                    byte[] bytes = await _pipe.ReadAsync(cancellationToken);
                    bytesWrittenSoFar = previousBuffer.Count;
                    previousBuffer.AddRange(bytes);
                    endIndex = IndexOfSequence(previousBuffer.ToArray(),
                                               Constants.EndBreakpointMessage);
                }

                // Ensure we have a start to the breakpoint message.
                int startIndex = IndexOfSequence(previousBuffer.ToArray(),
                                                 Constants.StartBreakpointMessage);
                if (startIndex == -1)
                {
                    throw new InvalidOperationException("Invalid breakpoint message.");
                }

                var newBytes = previousBuffer.GetRange(
                    startIndex + Constants.StartBreakpointMessage.Length, endIndex - startIndex - Constants.StartBreakpointMessage.Length);
                _buffer.AddRange(previousBuffer.Skip(endIndex + Constants.EndBreakpointMessage.Length));
                return Breakpoint.Parser.ParseFrom(newBytes.ToArray());
            }
            finally
            {
                _semaphore.Release();
            }
        }

        /// <inheritdoc />
        public Task WriteBreakpointAsync(Breakpoint breakpoint, CancellationToken cancellationToken = default(CancellationToken))
        {
            List<byte> bytes = new List<byte>();
            bytes.AddRange(Constants.StartBreakpointMessage);
            bytes.AddRange(breakpoint.ToByteArray());
            bytes.AddRange(Constants.EndBreakpointMessage);
            return _pipe.WriteAsync(bytes.ToArray(), cancellationToken);
        }

        /// <summary>
        /// Get the start index of a sequence.
        /// </summary>
        /// <param name="array">The array of bytes to look for a sequence in.</param>
        /// <param name="sequence">The sequence to search for.</param>
        /// <returns>The start index of the first sequence or -1 if none is found.</returns>
        internal static int IndexOfSequence(byte[] array, byte[] sequence)
        {
            for (int i = 0; i < array.Length - sequence.Length + 1; i++)
            {
                // This check could be slightly more efficient if we tracked
                // looked for the start of the sequence inside the match.
                // However given how small the sequence will be this should
                // be sufficient. 
                if (array[i] == sequence[0] && Match(array, i, sequence))
                {
                   return i;
                }
            }
            return -1;
        }

        /// <summary>
        /// Checks if the sequence is at the index of the given array.
        /// </summary>
        /// <returns>True if the there is a match.</returns>
        private static bool Match(byte[] array, int index, byte[] sequence)
        {
            for (int i = 0; i < sequence.Length; i++)
            {
                if (array[i + index] != sequence[i])
                {
                    return false;
                }
            }
            return true;
        }

        /// <inheritdoc />
        public void Dispose() => _pipe.Dispose();
    }
}
