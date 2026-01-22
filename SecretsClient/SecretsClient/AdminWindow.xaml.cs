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
using System.Net.Http;
using System.Text;
using Newtonsoft.Json;


namespace SecretsClient
{
    /// <summary>
    /// Логика взаимодействия для AdminWindow.xaml
    /// </summary>
    public partial class AdminWindow : Window
    {
        private readonly HttpClient client = new HttpClient();
        private string adminUsername = "admin";
        private string adminPassword = "adminpass";

        public AdminWindow()
        {
            InitializeComponent();
            LoadUsers();
            LoadSecrets();
            LoadStatistics();
        }

        private StringContent CreateAuthContent(object extra = null)
        {
            var data = new Dictionary<string, object>
        {
            { "username", adminUsername },
            { "password", adminPassword }
        };

            if (extra != null)
            {
                foreach (var prop in extra.GetType().GetProperties())
                    data[prop.Name] = prop.GetValue(extra);
            }

            return new StringContent(
                JsonConvert.SerializeObject(data),
                Encoding.UTF8,
                "application/json"
            );
        }
        private async void LoadUsers()
        {
            var response = await client.GetAsync("http://localhost:8080/api/users",
                HttpCompletionOption.ResponseContentRead,
                CreateAuthContent());

            var json = await response.Content.ReadAsStringAsync();
            dynamic result = JsonConvert.DeserializeObject(json);

            UsersGrid.ItemsSource = result.data.users;
        }
        private async void AddUser_Click(object sender, RoutedEventArgs e)
        {
            var content = new StringContent(JsonConvert.SerializeObject(new
            {
                username = "newuser",
                password = "12345",
                role = "user"
            }), Encoding.UTF8, "application/json");

            await client.PostAsync("http://localhost:8080/api/users", content);
            LoadUsers();
        }
        private async void LoadSecrets()
        {
            var content = CreateAuthContent();
            var response = await client.GetAsync("http://localhost:8080/api/secrets",
                HttpCompletionOption.ResponseContentRead,
                content);

            var json = await response.Content.ReadAsStringAsync();
            dynamic result = JsonConvert.DeserializeObject(json);

            SecretsGrid.ItemsSource = result.data.secrets;
        }
        private async void AddSecret_Click(object sender, RoutedEventArgs e)
        {
            var content = new StringContent(JsonConvert.SerializeObject(new
            {
                owner_id = 1,
                secret_value = "test",
                secret_type = "password"
            }), Encoding.UTF8, "application/json");

            await client.PostAsync("http://localhost:8080/api/secrets", content);
            LoadSecrets();
        }
        private async void EditSecret_Click(object sender, RoutedEventArgs e)
        {
            if (SecretsGrid.SelectedItem == null) return;

            dynamic secret = SecretsGrid.SelectedItem;

            var content = new StringContent(JsonConvert.SerializeObject(new
            {
                secret_value = "updated",
                secret_type = "password"
            }), Encoding.UTF8, "application/json");

            await client.PutAsync($"http://localhost:8080/api/secrets/{secret.id_secrets}", content);
            LoadSecrets();
        }
        private async void DeleteSecret_Click(object sender, RoutedEventArgs e)
        {
            if (SecretsGrid.SelectedItem == null) return;

            dynamic secret = SecretsGrid.SelectedItem;
            await client.DeleteAsync($"http://localhost:8080/api/secrets/{secret.id_secrets}");
            LoadSecrets();
        }
        private async void LoadStatistics()
        {
            var response = await client.GetAsync("http://localhost:8080/api/statistics");
            var json = await response.Content.ReadAsStringAsync();
            dynamic stats = JsonConvert.DeserializeObject(json).data;

            TotalActions.Text = $"Всего действий: {stats.total_actions}";
            UniqueUsers.Text = $"Уникальных пользователей: {stats.unique_users}";
            FirstUser.Text = $"Первый пользователь: {stats.first_active_user}";
            LastUser.Text = $"Последний пользователь: {stats.last_active_user}";
        }

    }
