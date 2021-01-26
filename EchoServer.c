#include "EchoServer.h"

int main(int argc, char ** argv)
{
  printf("| Welcome to this wonderful C server! |\n\n");

  int main_socket, cli, pid;
  struct TextfileData system_config_data;
  struct sockaddr_in client;
  unsigned int sockaddr_len = sizeof(struct sockaddr_in);

  setup_server(&system_config_data); 

  printf("---------------------------------------\n");
  printf("|CURRENT SERVER SETUP: \n");
  printf("|Port Number: %d\n", system_config_data.port_number);
  printf("|Document Root: %s\n", system_config_data.document_root);
  printf("|Default Web Page: %s\n", system_config_data.default_web_page);
  printf("---------------------------------------\n\n");

  main_socket = setup_socket(system_config_data.port_number, MAX_CLIENTS);

  while (1)  {

    if ( (cli = accept(main_socket, (struct sockaddr *)&client, &sockaddr_len)) < 0) {
      perror("ERROR on accept");
      exit(1);
    }

    pid = fork();
    if (pid < 0){
      perror("ERROR on fork");
      exit(1);
    }

    if (pid == 0) {
      close(main_socket);
      client_handler(cli, &system_config_data);
      //close(cli);
      exit(0);
    }

    if (pid > 0) {
      close(cli);
      waitpid(0, NULL, WNOHANG);
    }
  }
}

void deleteSubstring(char *original_string,const char *sub_string) {
  while( (original_string=strstr(original_string,sub_string)) )
    memmove(original_string,original_string+strlen(sub_string),1+strlen(original_string+strlen(sub_string)));
}

void client_handler(int client, struct TextfileData *config_data) {

  struct HTTP_RequestParams request_params;
  int data_len, status_code;
  char data[MAX_DATA], absolute_file_path[MAX_PATH_LENGTH];

  if ( (data_len = recv(client, data, MAX_DATA, 0)) < 0){
    perror("Recv: ");
    exit(-1);
  }

  if (data_len) {
    extract_request_parameters((char *)&data, &request_params);
    printf("- Result parameters from client process ID %d METHOD: %s URI: %s VERSION: %s\n", (int) getpid(), request_params.method, request_params.URI, request_params.httpversion);
  
    if (validate_request_headers(&request_params, &status_code))
      send_response(client, status_code, &request_params, (char *)&absolute_file_path);
    else {
      handle_file_serving( (request_params.URI), (char *)&absolute_file_path, config_data, &status_code);
      send_response(client, status_code, &request_params, (char *)&absolute_file_path);
    }
    free(request_params.method);
    free(request_params.URI);
    free(request_params.httpversion);
  }

}

void send_response(int client, int status_code, struct HTTP_RequestParams *params, char *full_path) {

  char invalid_version[] = "HTTP/1.1 400 Bad Request: Invalid Version: %s\r\n";
  char invalid_uri[] = "HTTP/1.1 400 Bad Request: Invalid URI: %s\r\n";
  char invalid_method[] = "HTTP/1.1 400 Bad Request: Invalid Method: %s\r\n";

  char not_found[] = "HTTP/1.1 404 Not Found:%s\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html><html><head><title>404 Not Found</title>"
    "<body><h1>404 Not Found:%s</h1></body></html>\r\n";

  char not_implemented[] = "HTTP/1.1 501 Not Implemented: %s\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html><html><head><title>501 Not Implemented</title>"
    "<body><h1>501 Not Implemented: %s</h1></body></html>\r\n";

  char actual_response[sizeof(not_implemented) + (2*(sizeof(params->URI))) +256];

  memset(&actual_response, 0, sizeof(actual_response));

  switch (status_code)
  {
    case 501:
      snprintf(actual_response, sizeof(actual_response), not_implemented, params->URI, params->URI);
      write(client, actual_response, sizeof(actual_response));
      break;
    case 404:
      snprintf(actual_response, sizeof(actual_response), not_found, params->URI, params->URI);
      write(client, actual_response, sizeof(actual_response));
      break;
    case 200:
      construct_file_response(full_path, client);
      break;
    case 4001:
      printf("This is a bad http method\n");
      snprintf(actual_response, ( sizeof(invalid_method) + ( (sizeof(params->method))*2)), invalid_method, params->method, params->method);
      write(client,actual_response, ( sizeof(invalid_method) + ( (sizeof(params->method))*2)));
      break;
    case 4002:
      printf("This is a bad http version\n");
      snprintf(actual_response, sizeof(actual_response), invalid_version, params->httpversion);
      write(client,actual_response, sizeof(actual_response));
      break;
    case 4003:
      printf("This is a bad http uri\n");
      snprintf(actual_response, sizeof(actual_response), invalid_uri, params->method);
      write(client,actual_response, sizeof(actual_response));
      break;
  }
}

void construct_file_response(char *full_path, int client) {

  char *user_request_extension;
  char buffer[1024];
  long file_size;
  FILE *requested_file;
  size_t read_bytes, total_read_bytes;
  user_request_extension = strchr(full_path, '.');

  char png_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: image/png; charset=UTF-8\r\n\r\n";
  char gif_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: image/gif; charset=UTF-8\r\n\r\n";
  char html_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: text/html; charset=UTF-8\r\n\r\n";
  char text_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
  char css_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: text/css; charset=UTF-8\r\n\r\n";
  char jpg_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: image/jpeg; charset=UTF-8\r\n\r\n";
  char htm_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: text/html; charset=UTF-8\r\n\r\n";
  char js_response[] = "HTTP/1.1 200 OK:\r\n" "Content-Type: text/javascript; charset=UTF-8\r\n\r\n";


  requested_file = fopen(full_path, "r");

  fseek(requested_file, 0, SEEK_END);
  file_size = ftell(requested_file);
  fseek(requested_file, 0, SEEK_SET);

 
  if ( (strcmp(user_request_extension, ".png")) == 0)
    send(client, png_response, sizeof(png_response)-1, 0);

  if ( (strcmp(user_request_extension, ".gif")) == 0)
    send(client, gif_response, sizeof(gif_response)-1, 0);

  if ( (strcmp(user_request_extension, ".html")) == 0)
    send(client, html_response, sizeof(html_response)-1, 0);
  
  if ( (strcmp(user_request_extension, ".jpg")) == 0)
    send(client, jpg_response, sizeof(jpg_response)-1, 0);
  
  if ( (strcmp(user_request_extension, ".text")) == 0)
    send(client, text_response, sizeof(text_response)-1, 0);
  
  if ( (strstr(user_request_extension, ".css")) != NULL)
    send(client, css_response, sizeof(css_response)-1, 0);
  
  if ( (strcmp(user_request_extension, ".htm")) == 0)
    send(client, htm_response, sizeof(htm_response)-1, 0);
  if ( (strstr(user_request_extension, ".js")) != NULL)
    send(client, js_response, sizeof(js_response)-1, 0);
  
  total_read_bytes = 0;
  while (!feof(requested_file)) {
    read_bytes = fread(buffer, 1, 1024, requested_file);
    total_read_bytes += read_bytes;
    send(client, buffer, read_bytes, 0);
  }
  fclose(requested_file);

}

int handle_file_serving(char *path, char *body, struct TextfileData *config_data, int *result_status) {

  int file_supported, i;
  file_supported = FALSE;
  char user_request_file_path[PATH_MAX + 1];
  char *user_request_extension;
  struct stat buffer;

  strcpy(user_request_file_path, config_data->document_root);
  strcat(user_request_file_path, path);
  user_request_extension = strrchr(user_request_file_path, '.');

  if ( ((strcmp(path, "/index")) == 0) || ((strcmp(path,"/")) ==0) || ((strcmp(path,"/index.html")) ==0)  ) {
    if ((strcmp(path, "/index")) == 0)
      strcat(user_request_file_path, ".html");
    if ((strcmp(path,"/")) ==0)
      strcat(user_request_file_path, "index.html");
    *result_status = 200;
    strcpy(body, user_request_file_path);
    return 0;
  }
  
  if (!user_request_extension) {
    *result_status = 404;
    strcpy(body, user_request_file_path);
    return 0;
  }

  for (i = 0; i < NUM_OF_FILE_TYPES; i++) {
    if ( (strcmp(user_request_extension, config_data->extensions[i])) == 0)
      file_supported = TRUE;
  }
  if (!file_supported) {
    *result_status = 501;
    strcpy(body, user_request_file_path);
    return(0);
  }

  if ( (stat (user_request_file_path, &buffer) == 0)) {
    *result_status = 200;
    strcpy(body, user_request_file_path);
    return 0;
  }
  else {
    *result_status = 404;
    strcpy(body, user_request_file_path);
    return 0;
  }
}

int validate_request_headers(struct HTTP_RequestParams *params, int *decision) {

  if ( (strcmp(params->method, "GET")) != 0) {
    printf("Invalid Method: %s\n", params->method);
    *decision = 4001;
    return 1;
  }

  if (((strncmp(params->httpversion, "HTTP/1.1",8 )) != 0)  && ((strcmp(params->httpversion, "HTTP/1.0")) != 0)) {
    printf("Invalid Version: %s\n", params->httpversion);
    *decision = 4002;
    return 1;
  }
  return 0;
}

void extract_request_parameters(char *response, struct HTTP_RequestParams *params) {
  char *saveptr, *the_path;

  the_path = strtok_r(response, "\n", &saveptr);

  the_path = strtok_r(the_path, " ", &saveptr);
  params->method = malloc(strlen(the_path)+1);
  strcpy(params->method, the_path);

  the_path = strtok_r(NULL, " ", &saveptr);
  params->URI = malloc(strlen(the_path)+1);
  strcpy(params->URI, the_path);

  params->httpversion = malloc(strlen(saveptr)+1);
  strcpy(params->httpversion, saveptr);
}

void setup_server(struct TextfileData *config_data) {

  char *current_path, *saveptr, *current_line;
  char buff[PATH_MAX + 1], conf_file_path[PATH_MAX + 1], read_line[200];
  int counter = 1;

  current_path = getcwd(buff, PATH_MAX + 1);
  strcpy(conf_file_path, current_path);
  strcat(conf_file_path, "/ws.conf");


  FILE *config_file;
  config_file = fopen(conf_file_path, "r");
  if (config_file == NULL) {
    perror("Opening conf file: ");
    exit(-1);
  }

  while (fgets (read_line,200, config_file) != NULL)
  {
    if (counter == 2) {
      current_line = strtok_r(read_line, " ", &saveptr);
      config_data->port_number = atoi(saveptr);
    }
    if (counter == 4) {
      current_line = strtok_r(read_line, " ", &saveptr);
      deleteSubstring(saveptr, "\"");
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->document_root, saveptr);
    }
    if (counter == 6) {
      current_line = strtok_r(read_line, " ", &saveptr);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->default_web_page, saveptr);
    }
    if (counter == 8) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[0], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[0], saveptr);
    }
    if (counter == 9) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[1], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[1], saveptr);
    }
    if (counter == 10) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[2], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[2], saveptr);
    }
    if (counter == 11) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[3], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[3], saveptr);
    }
    if (counter == 12) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[4], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[4], saveptr);
    }
    if (counter == 13) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[5], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[5], saveptr);
    }
    if (counter == 14) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[6], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[6], saveptr);
    }
    if (counter == 15) {
      current_line = strtok_r(read_line, " ", &saveptr);
      strcpy(config_data->extensions[7], current_line);
      deleteSubstring(saveptr, "\n");
      strcpy(config_data->encodings[7], saveptr);
    }
    counter++;
  }
  fclose(config_file);
}

int setup_socket(int port_number, int max_clients)
{
  struct sockaddr_in server;
  int sock;

  server.sin_family = AF_INET;
  server.sin_port = htons(port_number);
  server.sin_addr.s_addr = INADDR_ANY;
  memset(server.sin_zero, '\0', sizeof(server.sin_zero));

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
    perror("server socket: ");
    exit(-1);
  }


  if ((bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == ERROR) {
    perror("bind : ");
    exit(-1);
  }

  if ((listen(sock, max_clients)) == ERROR) {
    perror("Listen");
    exit(-1);
  }
  return sock;
}
