using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace SecretsClient
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        ApiService api = new ApiService();
        public MainWindow()
        {
            InitializeComponent();
        }
        private async void Login_Click(object sender, RoutedEventArgs e)
        {
            bool success = await api.Login(
                LoginBox.Text,
                PasswordBox.Password
            );

            if (success)
            {
                new SecretsWindow().Show();
                Close();
            }
            else
            {
                MessageBox.Show("Ошибка входа");
            }
        }
    }
}