using System;
using System.Collections.Generic;
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
using System.Windows.Shapes;

namespace SecretsClient
{
    /// <summary>
    /// Логика взаимодействия для SecretsWindow.xaml
    /// </summary>
    public partial class SecretsWindow : Window
    {
        ApiService api = new ApiService();

        public SecretsWindow()
        {
            InitializeComponent();
            LoadSecrets();
        }

        private async void LoadSecrets()
        {
            SecretsGrid.ItemsSource = await api.GetSecrets();
        }

        private void Refresh_Click(object sender, RoutedEventArgs e)
        {
            LoadSecrets();
        }
    }
}
