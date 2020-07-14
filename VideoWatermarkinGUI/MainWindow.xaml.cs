using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
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

namespace VideoWatermarkinGUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        string inputFilePath = "";
        string outputFilePath = "";

        Thread workerThread;
        Process currentProcess;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void ParseStdOut(string line)
        {
            if (line.StartsWith("Frame ") && line.Contains("/"))
            {
                try
                {
                    string numstr = line.Substring(6);
                    string[] nump = numstr.Split("/");
                    int current = int.Parse(nump[0].Trim());
                    int total = int.Parse(nump[1].Trim());
                    EncodeProgressBar.Maximum = total;
                    EncodeProgressBar.Value = current;
                } catch (Exception e)
                {

                }
            }
            EncodeConsoleOutput.Text += line + "\n";
            EncodeConsoleOutput.ScrollToEnd();
        }

        private string ParseEncodeArgument()
        {
            string arg = "-m encode ";
            if (string.IsNullOrEmpty(inputFilePath))
            {
                throw new ArgumentException("输入文件不能为空");
            }
            else
            {
                arg += "-i \"" + inputFilePath + "\" ";
            }

            if (string.IsNullOrEmpty(outputFilePath))
            {
                throw new ArgumentException("输出文件不能为空");
            }
            else
            {
                arg += "-o \"" + outputFilePath + "\" ";
            }

            if (string.IsNullOrEmpty(WatermarkContent.Text))
            {
                throw new ArgumentException("水印内容不能为空");
            }

            arg += "-w \"" + WatermarkContent.Text + "\" ";

            double position = 0.2222;
            if(!double.TryParse(WatermarkPosition.Text, out position) || position > 1 || position < -1)
            {
                throw new ArgumentException("位置超出范围");
            }
            arg += "-p " + position.ToString("N4") + " ";

            if (UseYUVModeCheckbox.IsChecked != null && (bool)UseYUVModeCheckbox.IsChecked)
            {
                arg += "-v ";
            }

            if (DumpJPGCheckbox.IsChecked != null && (bool)DumpJPGCheckbox.IsChecked)
            {
                arg += "-d ";
            }

            if (SetFrameRangeCheckbox.IsChecked != null && (bool)SetFrameRangeCheckbox.IsChecked)
            {
                int frame = 100;
                if (!int.TryParse(FrameRangeTextBox.Text, out frame) || frame < 1)
                {
                    throw new ArgumentException("帧数超出范围");
                }
                arg += "-f " + frame.ToString() + " ";
            }

            ParseStdOut("运行参数:" + arg);
            return arg;
        }

        private string ParseDecodeArgument()
        {
            string arg = "-m decode ";
            if (string.IsNullOrEmpty(inputFilePath))
            {
                throw new ArgumentException("输入文件不能为空");
            }
            else
            {
                arg += "-i \"" + inputFilePath + "\" ";
            }

            if (UseYUVModeCheckbox.IsChecked != null && (bool)UseYUVModeCheckbox.IsChecked)
            {
                arg += "-v ";
            }

            return arg;
        }

        private void NumberValidationTextBox(object sender, TextCompositionEventArgs e)
        {
            Regex regex = new Regex("[^0-9]+");
            e.Handled = regex.IsMatch(e.Text);
        }

        private void EncodeFileSelectButton_Click(object sender, RoutedEventArgs e)
        {
            Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
            dlg.DefaultExt = ".mp4";
            dlg.Filter = "Video Files (.mp4)|*.mp4|All files (*.*)|*.*";

            Nullable<bool> result = dlg.ShowDialog();

            if (result == true)
            {
                string filename = dlg.FileName;
                inputFilePath = filename;
                EncodeFileSelectTextBox.Text = filename;
            }
        }
        private void EncodeOutputSelectButton_Click(object sender, RoutedEventArgs e)
        {
            Microsoft.Win32.SaveFileDialog dlg = new Microsoft.Win32.SaveFileDialog();
            dlg.FileName = "output";
            dlg.DefaultExt = ".avi";
            dlg.Filter = "Video Files (.avi)|*.avi";

            Nullable<bool> result = dlg.ShowDialog();

            if (result == true)
            {
                string filename = dlg.FileName;
                outputFilePath = filename;
                EncodeOutputSelectTextBox.Text = filename;
            }
        }

        private void StartEncodeButton_Click(object sender, RoutedEventArgs e)
        {
            if (!StartEncodeButton.Content.Equals("开始处理"))
            {
                try
                {
                    Process.GetProcessById(currentProcess.Id);
                    currentProcess.Kill();
                    currentProcess.Close();
                } catch (Exception exc) { }
                

                currentProcess = null;
                workerThread = null;
                StartEncodeButton.Content = "开始处理";
            }
            else
            {
                if (workerThread != null )
                {
                    if (workerThread.IsAlive)
                    {
                        workerThread.Abort();
                    }
                    workerThread = null;
                }
                
                workerThread = new Thread(() =>
                {
                    Process process = new Process();
                    process.StartInfo.FileName = "VideoWatermarking.exe";
                    this.Dispatcher.Invoke(() =>
                    {
                        try
                        {
                            process.StartInfo.Arguments = ParseEncodeArgument();
                        }
                        catch (Exception e)
                        {
                            MessageBox.Show(e.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                            return;
                        }
                    });
                    process.StartInfo.UseShellExecute = false;
                    process.StartInfo.RedirectStandardOutput = true;
                    process.StartInfo.RedirectStandardError = true;
                    process.StartInfo.StandardOutputEncoding = Encoding.UTF8;
                    process.StartInfo.StandardErrorEncoding = Encoding.UTF8;
                    process.StartInfo.UseShellExecute = false;
                    process.StartInfo.CreateNoWindow = true;
                    process.OutputDataReceived += new DataReceivedEventHandler((sender, e) =>
                    {
                        if (!String.IsNullOrEmpty(e.Data))
                        {
                            this.Dispatcher.Invoke(() =>
                            {
                                ParseStdOut(e.Data);
                            });
                        }
                    });
                    process.ErrorDataReceived += new DataReceivedEventHandler((sender, e) =>
                    {
                        if (!String.IsNullOrEmpty(e.Data))
                        {
                            this.Dispatcher.Invoke(() =>
                            {
                                ParseStdOut(e.Data);
                            });
                        }
                    });
                    try
                    {
                        currentProcess = process;
                        process.Start();

                        this.Dispatcher.Invoke(() =>
                        {
                            StartEncodeButton.Content = "停止处理";
                        });

                        process.BeginOutputReadLine();
                        process.BeginErrorReadLine();
                        process.WaitForExit();
                        process.Close();
                        this.Dispatcher.Invoke(() =>
                        {
                            StartEncodeButton.Content = "开始处理";
                        });
                        process.Dispose();
                        process = null;
                    }
                    catch (Exception e)
                    {
                        MessageBox.Show(e.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    }

                });


                workerThread.Start();
            }
        }

        private void StartDecodeButton_Click(object sender, RoutedEventArgs e)
        {
            Thread decodeThread = new Thread(() =>
            {
                if(File.Exists("decoded_0.jpg"))
                {
                    File.Delete("decoded_0.jpg");
                }
                Process process = new Process();
                process.StartInfo.FileName = "VideoWatermarking.exe";
                this.Dispatcher.Invoke(() =>
                {
                    try
                    {
                        process.StartInfo.Arguments = ParseDecodeArgument();
                    }
                    catch (Exception e)
                    {
                        MessageBox.Show(e.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                        return;
                    }
                });
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.RedirectStandardError = true;
                process.StartInfo.StandardOutputEncoding = Encoding.UTF8;
                process.StartInfo.StandardErrorEncoding = Encoding.UTF8;
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.CreateNoWindow = true;

                try
                {
                    process.Start();

                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();
                    process.WaitForExit();
                    int exitCode = process.ExitCode;
                    process.Close();
                    this.Dispatcher.Invoke(() =>
                    {
                        ResultImage.Source = new BitmapImage(new Uri(System.IO.Path.Combine(inputFilePath + "_decoded_0.jpg")));
                    });
                }
                catch (Exception e)
                {
                    MessageBox.Show(e.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                }

            });

            decodeThread.Start();
        }

        private void DecodeFileSelectButton_Click(object sender, RoutedEventArgs e)
        {
            Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
            dlg.DefaultExt = ".jpg";
            dlg.Filter = "Image Files (.jpg;.png)|*.mp4;*.png|All files (*.*)|*.*";

            Nullable<bool> result = dlg.ShowDialog();

            if (result == true)
            {
                string filename = dlg.FileName;
                inputFilePath = filename;
                DecodeFileSelectTextBox.Text = filename;
            }
        }
    }
}
