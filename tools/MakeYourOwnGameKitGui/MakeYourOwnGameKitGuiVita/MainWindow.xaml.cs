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

        private void CreateVPKButton_Click(object sender, RoutedEventArgs e)
        {
            var command = "make_vita.bat \"" + GameName.Text + "\" " + GameTitleID.Text + " " + GameVersion.Text;
            var processInfo = new ProcessStartInfo("cmd.exe", "/c " + command);
            var process = Process.Start(processInfo);
            process.WaitForExit();
            process.Close();

            MessageBox.Show("VPK creation complete. In case of success, output path is: \n\n" + System.IO.Path.GetDirectoryName(AppDomain.CurrentDomain.BaseDirectory) + "\\" + GameTitleID.Text + ".vpk",
                                          "Process Complete",
                                          MessageBoxButton.OK,
                                          MessageBoxImage.Information);
        }
    }
}
