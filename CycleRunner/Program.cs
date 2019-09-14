using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

namespace CycleRunner
{
    internal class Program
    {
        public static int Main(string[] args)
        {
            var binaryDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var solutionDir = Path.GetFullPath(Path.Combine(binaryDirectory, "../../.."));
            var targetProgram = Path.Combine(solutionDir, "Debug/NetRuntimeWaiter.exe");
            for (int i = 0; i < 100000; ++i)
            {
                Console.WriteLine("Try " + i);
                var startInfo = new ProcessStartInfo(targetProgram, args.Length > 0 && args[0] == "b" ? "b" : "a")
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true
                };
                var process = Process.Start(startInfo);
                string line;
                while ((line = process.StandardOutput.ReadLine()) != null)
                {
                    Console.WriteLine(line);
                }

                process.WaitForExit();
                if (process.ExitCode != 0)
                {
                    Console.WriteLine("ERROR CODE: " + process.ExitCode);
                    return 1;
                }
            }

            return 0;
        }
    }
}