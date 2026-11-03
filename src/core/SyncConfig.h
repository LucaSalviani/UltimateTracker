#pragma once

// Proyecto de Supabase donde se juntan los datos de la PC y el celu.
// La clave "publishable" está hecha para ir dentro de las apps: sola no da acceso
// a nada; cada usuario solo puede leer y escribir sus propias filas (ver las
// políticas en tools/supabase.sql).
namespace syncconfig {
inline const char* kBaseUrl = "https://ojyuulebjymsmgsbymzb.supabase.co";
inline const char* kPublishableKey = "sb_publishable_1eWKmP5Tj1h1p57cAfTYHg_16SqHkIK";
}
