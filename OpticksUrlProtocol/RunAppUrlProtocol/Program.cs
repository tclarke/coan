#region Namespace Inclusions
using System;
using System.IO;
using System.Xml;
using System.Reflection;
using System.Diagnostics;
using System.Windows.Forms;
using System.Text.RegularExpressions;
#endregion

namespace RunAppUrlProtocol
{
	class Program
	{
		static void Main(string[] args)
		{
			// The URL handler for this app
			string prefix = "runapp://";

			// The name of this app for user messages
			string title = "Opticks URL Protocol Handler";

            // Example: runapp://opticks/ C:/Data/DigitalGlobe/AST_L1B_00304282006155031_05022006131334_SWIR_Swath.tif C:/Data/DigitalGlobe/AST_L1B_00304282006155031_05022006131334_TIR_Swath.tif
			
            // Verify the command line arguments
			if (args.Length == 0)
            {
                MessageBox.Show("Syntax Error:\nExpected: runapp://opticks/ <args>\n", title, MessageBoxButtons.OK, MessageBoxIcon.Error); return;
            }
            else if (!args[0].StartsWith(prefix))
            { 
                MessageBox.Show("Invalid Prefix:\nExpected: runapp://opticks/ <args>\nReceived: " + args[0], title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; 
            }

			// Extract the application name "opticks" from the URL
            string key = Regex.Match(args[0], @"(?<=://).+?(?=:|/|\Z)").Value;
            
            // Build the target arguments list
            // This expression extracts the first argument to send to Opticks in case there's not a space separating them
            string reg = String.Format("(?<={0}/).+?\\Z", key);
            string arg = Regex.Match(args[0], reg).Value;
            
            // Build the list of arguments to pass to our application
            arg += " ";
            for (int i = 1; i < args.Length; i++)
            {
                arg += args[i] + " ";
            }

            // Path to the configuration file
			string file = Path.Combine(Path.GetDirectoryName(
				Assembly.GetExecutingAssembly().Location), "RegisteredApps.xml");

			// Verify the config file exists
			if (!File.Exists(file))
			{ MessageBox.Show("Could not find configuration file.\n" + file, title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

			// Load the config file
			XmlDocument xml = new XmlDocument();
			try { xml.Load(file); }
			catch (XmlException e) 
			{ MessageBox.Show(String.Format("Error loading the XML config file.\n{0}\n{1}", file, e.Message), title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

			// Locate the app to run
			XmlNode node = xml.SelectSingleNode(
				String.Format("/RunApp/App[@key='{0}']", key));

			// If the app is not found, let the user know
			if (node == null)
            { MessageBox.Show("Key not found in registered applications: " + key, title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
	
			// Resolve the target app name
			string target = Environment.ExpandEnvironmentVariables(
				node.SelectSingleNode("@target").Value);

			// Pull other command line args for the target app if they exist
            //string procargs = Environment.ExpandEnvironmentVariables( node.SelectSingleNode("@args") != null ? node.SelectSingleNode("@args").Value : "");

            // Verify the target exists
            if (!File.Exists(target))
            { MessageBox.Show("Could not find target application.\n" + target, title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

			// Start the application
            Process.Start(target, arg);
		}
	}
}
