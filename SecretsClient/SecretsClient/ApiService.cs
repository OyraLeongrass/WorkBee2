using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Http;
using System.Text;
using System.Text.Json;

namespace SecretsClient
{
    public class ApiService
    {
        private static readonly HttpClient client = new HttpClient
        {
            BaseAddress = new Uri("http://localhost:8080")
        };

        public async Task<bool> Login(string username, string password)
        {
            var json = JsonSerializer.Serialize(new
            {
                username,
                password
            });

            var response = await client.PostAsync(
                "/api/auth/login",
                new StringContent(json, Encoding.UTF8, "application/json")
            );

            return response.IsSuccessStatusCode;
        }

        public async Task<List<Secret>> GetSecrets()
        {
            var response = await client.GetAsync("/api/secrets");
            var json = await response.Content.ReadAsStringAsync();

            using var doc = JsonDocument.Parse(json);
            var data = doc.RootElement.GetProperty("data")
                                      .GetProperty("secrets");

            return JsonSerializer.Deserialize<List<Secret>>(data.ToString());
        }

        public async Task AddSecret(int ownerId, string secret, string type)
        {
            var json = JsonSerializer.Serialize(new
            {
                owner_id = ownerId,
                secret = secret,
                secret_type = type
            });

            await client.PostAsync(
                "/api/secrets",
                new StringContent(json, Encoding.UTF8, "application/json")
            );
        }
    }
}
