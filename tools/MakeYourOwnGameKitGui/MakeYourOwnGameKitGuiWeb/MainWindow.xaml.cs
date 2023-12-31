using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace MakeYourOwnGameKitGuiWeb
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void CreateWebButton_Click(object sender, RoutedEventArgs e)
        {
            var command = "make_web.bat " + GameName.Text;
            var processInfo = new ProcessStartInfo("cmd.exe", "/c " + command);
            processInfo.WorkingDirectory = "web";
            var process = Process.Start(processInfo);
            process.WaitForExit();
            process.Close();

            MessageBox.Show("Web build creation complete. Zip output path (if zipping was successful) is: \n\n" + System.IO.Path.GetDirectoryName(AppDomain.CurrentDomain.BaseDirectory) + "\\" + GameName.Text + ".zip",
                                          "Process Complete",
                                          MessageBoxButton.OK,
                                          MessageBoxImage.Information);
        }
    }
}
