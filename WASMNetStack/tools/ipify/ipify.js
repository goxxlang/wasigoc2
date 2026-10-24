fetch("/?format=json", { cache: "no-store" })  .then(response => response.json())  .then(data => console.log("Visible IP:", data.ip))  .catch(error => console.error("Request failed:", error));
