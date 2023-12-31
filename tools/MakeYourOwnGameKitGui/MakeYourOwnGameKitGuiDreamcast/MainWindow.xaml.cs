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

namespace MakeYourOwnGameKitGuiDreamcast
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

        private void CreateCDIButton_Click(object sender, RoutedEventArgs e)
        {
            var command = "make_cdi.bat " + CDLabelName.Text + " " + CDIName.Text;
            var processInfo = new ProcessStartInfo("cmd.exe", "/c " + command);
            processInfo.WorkingDirectory = "dreamcast";
            var process = Process.Start(processInfo);
            process.WaitForExit();
            process.Close();

            MessageBox.Show("CDI creation complete. Output path is: \n\n" + System.IO.Path.GetDirectoryName(AppDomain.CurrentDomain.BaseDirectory) + "\\dreamcast\\" + CDIName.Text + ".cdi",
                                          "Process Complete",
                                          MessageBoxButton.OK,
                                          MessageBoxImage.Information);
        }
    }
}
