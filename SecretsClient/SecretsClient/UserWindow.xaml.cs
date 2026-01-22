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
    /// Логика взаимодействия для UserWindow.xaml
    /// </summary>
    public partial class UserWindow : Window
    {
        private int _userId;

        public UserWindow(int userId)
        {
            InitializeComponent();
            _userId = userId;
            LoadSecrets();
        }

        private void LoadSecrets()
        {
            var secrets = Database.Instance.GetSecretsByUser(_userId);
            SecretsGrid.ItemsSource = secrets;
        }

        private void Search_Click(object sender, RoutedEventArgs e)
        {
            string pattern = SearchBox.Text.Trim();

            if (string.IsNullOrEmpty(pattern))
            {
                LoadSecrets();
                return;
            }

            var secrets = Database.Instance
                .SearchSecrets(pattern)
                .Where(s => s.owner_id == _userId)
                .ToList();

            SecretsGrid.ItemsSource = secrets;
        }

        private void AddSecret_Click(object sender, RoutedEventArgs e)
        {
            SecretEditWindow win = new SecretEditWindow(_userId);
            if (win.ShowDialog() == true)
            {
                LoadSecrets();
            }
        }

        private void EditSecret_Click(object sender, RoutedEventArgs e)
        {
            if (SecretsGrid.SelectedItem is Secret selected)
            {
                SecretEditWindow win = new SecretEditWindow(_userId, selected);
                if (win.ShowDialog() == true)
                {
                    LoadSecrets();
                }
            }
            else
            {
                MessageBox.Show("Выберите секрет");
            }
        }
    }

}
