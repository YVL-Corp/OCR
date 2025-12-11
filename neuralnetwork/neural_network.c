#include "neural_network.h" 


double sigmoid(double x)               // MAths function used in neural networks
{
    return 1.0 / (1.0 + exp(-x));
}

Network *create_network(int input, int hidden, int output)      // Initialization of the data type created in the header
{
    Network *net = (Network *)malloc(sizeof(Network));          // Allocate memory needed
    net->input_size = input;                                    // Initialize data
    net->hidden_size = hidden;
    net->output_size = output;

    // Allocate memory for the hidden neurons

    net->weights_input_hidden = malloc(input * sizeof(double*));
    for (int i = 0; i < input; i++) {
        net->weights_input_hidden[i] = malloc(hidden * sizeof(double));
    }
    
    

    net->weights_hidden_output = malloc(hidden * sizeof(double*));
    for (int i = 0; i < hidden; i++) {
        net->weights_hidden_output[i] = malloc(output * sizeof(double));
    }
    
    net->bias_hidden = (double *)malloc(hidden * sizeof(double));
    net->bias_output = (double *)malloc(output * sizeof(double));

    initialize_weights(net);                            // Then Initialize all the weights of the neurons
    return net;
}

void initialize_weights(Network *net) 
{
    static int seeded = 0;                          // Before initializing the seed, check if the seed has been calculated
    if (!seeded) {
        srand(time(NULL) + clock());                // Set the seed based on the current time so it will never be the same
        seeded = 1;
    }
    
    for (size_t i = 0; i < net->input_size; i++) 
    {
        for (size_t j = 0; j < net->hidden_size; j++) 
        {
            net->weights_input_hidden[i][j] = ((double)rand() / RAND_MAX) - 0.5;        // Initialize the weights with a double between -0.5 and 0.5
        }
    }
    
    for (size_t i = 0; i < net->hidden_size; i++) 
    {
        for (size_t j = 0; j < net->output_size; j++) 
        {
            net->weights_hidden_output[i][j] = ((double)rand() / RAND_MAX) - 0.5;       // same thing here
        }
    }
    
    for (size_t i = 0; i < net->hidden_size; i++) 
    {
        net->bias_hidden[i] = 0.0;                                                      // Then initialize the bias at 0, it will be changed when the neural network is used
    }
    
    for (size_t i = 0; i < net->output_size; i++) 
    {
        net->bias_output[i] = 0.0;
    }
}

void free_network(Network *net)         // Function to free all the memory allocated for the neural network 
{
    for (size_t i = 0; i < net->input_size; i++) 
    {
        free(net->weights_input_hidden[i]);
    }
    free(net->weights_input_hidden);
    
    for (size_t i = 0; i < net->hidden_size; i++) 
    {
        free(net->weights_hidden_output[i]);
    }
    free(net->weights_hidden_output);
    
    free(net->bias_hidden);
    free(net->bias_output);
    free(net);
}

double *forward(Network *net, double *input, double *hidden, double *output)            // Function that sends data through all the layers (input -> hidden -> output)
{

    // First, pass the data to the hidden layer from the input
    for (size_t i = 0; i < net->hidden_size; i++) 
    {
        hidden[i] = net->bias_hidden[i];
        for (size_t j = 0; j < net->input_size; j++) 
        {
            hidden[i] += input[j] * net->weights_input_hidden[j][i];        // Input data is modified based on the weights of the neurons to compute an output
        }
        hidden[i] = sigmoid(hidden[i]);
    }
    

    // Then iterate over the output layers
    for (int i = 0; i < net->output_size; i++)
    {
        output[i] = net->bias_output[i]; // Initialize output with the bias
        // Compute the sum with the weight as coefficient
        for (int j = 0; j < net->hidden_size; j++)
        {
            output[i] += hidden[j] * net->weights_hidden_output[j][i];
        }
    }


    if (net->output_size == 1) {
        // In case of a single output, we use the sigmoid function (e.g. for computing binary operations)
        output[0] = sigmoid(output[0]);
    } 
    else 
    {
        // Otherwise, use the softmax
        double max_val = output[0];
        for (size_t i = 1; i < net->output_size; i++) 
        {
            if (output[i] > max_val) max_val = output[i];
        }
        double sum = 0.0;
        for (size_t i = 0; i < net->output_size; i++) 
        {
            output[i] = exp(output[i] - max_val);
            sum += output[i];
        }
        for (size_t i = 0; i < net->output_size; i++) 
        {
            output[i] /= sum;
        }
    }
    return output;
}

void backpropagate(Network* net, double *input, double *target, double learning_rate)       //Function to propagate the errors back to correct the weights
{
    double *hidden = malloc(net->hidden_size * sizeof(double));             // Allocate memory for the hidden layer and output layer to be then filled when called with forward() 
    double *output = malloc(net->output_size * sizeof(double));
    
    forward(net, input, hidden, output);                                    // FIll the arrays
    
    double *output_errors = malloc(net->output_size * sizeof(double));
    if (net->output_size == 1) 
    {
        // If we use the sigmoid : gradient = (output - target) * sigmoid'
        output_errors[0] = (output[0] - target[0]) * output[0] * (1.0 - output[0]);
    }
    else 
    {
        // Otherwise for the softmax : gradient = output - target
        for (size_t i = 0; i < net->output_size; i++) 
        {
            output_errors[i] = output[i] - target[i];
        }
    }

    // Update weights and biases for hidden-output
    for (size_t i = 0; i < net->hidden_size; i++) 
    {
        for (size_t j = 0; j < net->output_size; j++) 
        {
            double gradient = hidden[i] * output_errors[j];
            net->weights_hidden_output[i][j] -= learning_rate * hidden[i] * output_errors[j];
        }
    }  


    for (int i = 0; i < net->output_size; i++) 
    {
        double gradient = output_errors[i];
        net->bias_output[i] -= learning_rate * gradient;
    }

    // Calculate hidden layer errors
    double *delta_hidden = malloc(net->hidden_size * sizeof(double));
    for (size_t i = 0; i < net->hidden_size; i++) {
        delta_hidden[i] = 0.0;
        
        // Propagate error from the output back towards the input
        for (size_t j = 0; j < net->output_size; j++) {
            delta_hidden[i] += output_errors[j] * net->weights_hidden_output[i][j];
        }
        
        // Multiply by the derivated of sigmoid
        delta_hidden[i] *= hidden[i] * (1.0 - hidden[i]);
    }

    // Update weights and biases for input-hidden
    for (size_t i = 0; i < net->input_size; i++) {
        for (size_t j = 0; j < net->hidden_size; j++) {
            double gradient = input[i] * delta_hidden[j];
            net->weights_input_hidden[i][j] -= learning_rate * gradient;
        }
    }
    for (size_t i = 0; i < net->hidden_size; i++) {
        net->bias_hidden[i] -= learning_rate * delta_hidden[i];
    }

    // Free allocated memory
    free(output_errors);
    free(delta_hidden);
    free(hidden);
    free(output);
}

void train(Network* net, TrainingExample* examples, size_t num_examples, int epochs, float learning_rate)        //Function to train the neural network with datasets (epochs)
{

    double last_accuracy = 0;
    printf("Called in the train function...\n");
    for (int epoch = 0; epoch < epochs; epoch++) 
    {
        for (size_t i = 0; i < num_examples; i++) 
        {
            double *target = calloc(net->output_size, sizeof(double)); // Create an array the size of the ouput with values initialized at 0
            target[examples[i].label] = 1.0; // One-hot encoding: the target we want the output to reach is 1.0 as they return a value between O and 1
            backpropagate(net, examples[i].pixels, target, learning_rate); // Start backpropagating with a set learning rate
            free(target); // Free allocated memory
        }

        // Display the neural network's accuracy every 100 epoch
        if ((epoch + 1) % 100 == 0) 
        {
            int correct = 0;            // Count the amount of correct guesses
            double *hidden = malloc(net->hidden_size * sizeof(double));
            double *output = malloc(net->output_size * sizeof(double));

            for (size_t i = 0; i < num_examples; i++) 
            {
                forward(net, examples[i].pixels, hidden, output);

                // Find predicted output
                int predicted = 0;
                double max_prob = output[0];
                for (int j = 1; j < net->output_size; j++) 
                {
                    if (output[j] > max_prob) 
                    {
                        max_prob = output[j];
                        predicted = j;
                    }
                }

                // Check if the prediction is correct
                if (predicted == examples[i].label) 
                {
                    correct++;
                }
            }
            free(hidden);
            free(output);

            // Calculate and display accuracy in percentages
            double accuracy = (double)correct / num_examples * 100.0;
        
            printf("Epoch %d/%d - Accuracy: %.2f%%\n", epoch + 1, epochs, accuracy);

            if (accuracy > 99) 
            {
                printf("!!! Reached > 99%% accuracy at %i epochs !!! \n", epoch);
                printf("Epoch %d/%d - Accuracy: %.2f%%\n", epoch + 1, epochs, accuracy);
                printf("Stopping early...\n");
                break;
            }
        }
    }
}