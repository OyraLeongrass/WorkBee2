using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SecretsClient
{
    public class Secret
    {
        public int id_secrets { get; set; }
        public int owner_id { get; set; }
        public string type { get; set; }
        public string created_at { get; set; }
    }
}
