TOKEN="eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJuYW1lIjoidXNlciIsInN1YiI6IjEyMzQ1Njc4OTAiLCJpYXQiOjE1MTYyMzkwMjJ9.tyU2qFaZXqQR5koc0ESegMYyGVwudHWHZJPMn6xXN8r8z4vpRpI3XqF0URqraQhTP4k6i72Uespk77Y1gqGYHg"
curl -D - -w "@../curl_format.txt" -H "Authorization: ${TOKEN}" 127.0.0.1:8080
